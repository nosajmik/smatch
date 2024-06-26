/*
 * Copyright (C) 2018 Oracle.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/copyleft/gpl.txt
 */

#include "smatch.h"
#include "smatch_extra.h"

static int my_id;
extern int second_half_id;
extern void set_spectre_first_half(struct expression *expr);

enum kernel_data_types {
	USER_DATA_TYPE = 0,
	HOST_DATA_TYPE = 1,
};

static int suppress_multiple = 1;
static int analyzed_data_type = USER_DATA_TYPE;

static int is_write(struct expression *expr)
{
	return 0;
}

static int is_read(struct expression *expr)
{
	struct expression *parent, *last_parent;
	struct statement *stmt;

	if (is_write(expr))
		return 0;

	last_parent = expr;
	while ((parent = expr_get_parent_expr(expr))){

		last_parent = parent;

		/* If we pass a value as a parameter that's a read, probably? */
//		if (parent->type == EXPR_CALL)
//			return 1;

		if (parent->type == EXPR_ASSIGNMENT) {
			if (parent->right == expr)
				return 1;
			if (parent->left == expr)
				return 0;
		}
		expr = parent;
	}

	stmt = expr_get_parent_stmt(last_parent);
	if (stmt && stmt->type == STMT_RETURN)
		return 1;

	return 0;
}

static int is_harmless(struct expression *expr)
{
	struct expression *tmp, *parent;
	struct statement *stmt;
	int count = 0;

	parent = expr;
	while ((tmp = expr_get_parent_expr(parent))) {
		if (tmp->type == EXPR_ASSIGNMENT || tmp->type == EXPR_CALL)
			return 0;
		parent = tmp;
		if (count++ > 4)
			break;
	}

	// Is the parent statement an if?
	stmt = expr_get_parent_stmt(parent);
	if (!stmt)
		return 0;
	if (stmt->type == STMT_IF && stmt->if_conditional == parent)
		return 1;
	if (stmt->type == STMT_ITERATOR &&
	    (stmt->iterator_pre_condition == parent ||
	     stmt->iterator_post_condition == parent))
		return 1;

	return 0;
}

static unsigned long long get_max_by_type(struct expression *expr)
{
	sval_t max = {
		.type = &ullong_ctype,
		.uvalue = -1ULL,
	};
	struct symbol *type;
	int cnt = 0;

	while (true) {
		expr = strip_parens(expr);
		type = get_type(expr);
		if (type && sval_type_max(type).uvalue < max.uvalue)
			max = sval_type_max(type);
		if (expr->type == EXPR_PREOP) {
			expr = expr->unop;
		} else if (expr->type == EXPR_BINOP) {
			if (expr->op == '%' || expr->op == '&')
				expr = expr->right;
			else
				return max.uvalue;
		} else {
			expr = get_assigned_expr(expr);
			if (!expr)
				return max.uvalue;
		}
		if (cnt++ > 5)
			return max.uvalue;
	}

	return max.uvalue;
}

static unsigned long long get_mask(struct expression *expr)
{
	struct expression *tmp;
	sval_t mask;
	int cnt = 0;

	expr = strip_expr(expr);

	tmp = get_assigned_expr(expr);
	while (tmp) {
		expr = tmp;
		if (++cnt > 3)
			break;
		tmp = get_assigned_expr(expr);
	}

	if (expr->type == EXPR_BINOP && expr->op == '&') {
		if (get_value(expr->right, &mask))  /* right is the common case */
			return mask.uvalue;
		if (get_value(expr->left, &mask))
			return mask.uvalue;
	}

	return ULLONG_MAX;
}

// Eliminates everything that is not an array access
// and not fillable with user controlled data. That is all
// the first part of check_spectre does. 
static void array_check(struct expression *expr)
{
	// printf("array_check called\n");
	struct expression_list *conditions;
	struct expression *array_expr, *offset;
	unsigned long long mask;
	int array_size;
	char *name;

	// Check this is an array, and is a possible path
	expr = strip_expr(expr);
	// printf("array check\n");
	if (!is_array(expr))
		return;

	// printf("possible path check\n");
	if (is_impossible_path())
		return;
	// printf("harmless check\n");
	if (is_harmless(expr))
		return;

	// Gets base pointer of array
	array_expr = get_array_base(expr);
	if (suppress_multiple && is_ignored_expr(my_id, array_expr)) {
		// printf("set spectre 1st half\n");
		set_spectre_first_half(expr);
		return;
	}

	// This is the index variable
	// printf("getting offset\n");
	offset = get_array_offset(expr);
	// switch(analyzed_data_type) {
	// 	// Is the data user-controlled?
	// 	case USER_DATA_TYPE:
	// 		// printf("user data type\n");
	// 		if (!is_user_rl(offset))
	// 			return;
	// 		break;
	// 	case HOST_DATA_TYPE:
	// 		// printf("host data type\n");
	// 		if (!is_host_rl(offset))
	// 			return;
	// 		break;
	// 	default:
	// 		return;
	// }

	if (is_nospec(offset))
		return;

	// printf("passed is_nospec\n");
	array_size = get_array_size(array_expr);
	if (array_size > 0 && get_max_by_type(offset) < array_size)
		return;
//	binfo = get_bit_info(offset);
//	if (array_size > 0 && binfo && binfo->possible < array_size)
//		return;

	mask = get_mask(offset);
	if (mask <= array_size)
		return;

	conditions = get_conditions(offset);

	name = expr_to_str(array_expr);
	// This doesn't matter too much -- just says that there
	// is an array access after an if-statement
	sm_warning("potential spectre issue '%s' [%s]%s",
	       name,
	       is_read(expr) ? "r" : "w",
	       conditions ? " (local cap)" : "");

	set_spectre_first_half(expr);
	if (suppress_multiple)
		add_ignore_expr(my_id, array_expr);
	free_string(name);
}

void check_spectre(int id)
{
	// printf("check_spectre called\n");
	my_id = id;

	// suppress_multiple = getenv("FULL_SPECTRE") == NULL;
	// if (getenv("ANALYZE_HOST_DATA"))
	// 	analyzed_data_type = HOST_DATA_TYPE;

	// if (option_project != PROJ_KERNEL)
	// 	return;

	// Whenever operation happens, array_check will be called
	add_hook(&array_check, OP_HOOK);
}
