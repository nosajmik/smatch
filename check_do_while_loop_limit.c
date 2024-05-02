/*
 * Copyright (C) 2021 Oracle.
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
#include <regex.h>

static int my_id;

static void double_deref_in_iter(struct expression *expr)
{
	expr = strip_expr(expr);

	// Get the complete conditional expression, not just
	// the variable it is conditioned on.
	struct expression *parent_expr = NULL;
	parent_expr = expr_get_parent_expr(expr);

	// Parent expression must be a single comparison, so this rules
	// out for-loops (they would be compound).
	if (!parent_expr || parent_expr->type != EXPR_COMPARE)
		return;

	// Try to find the name of the loop variable. Usually,
	// code standards put it on the left hand side of the loop
	// expression, but check the right afterwards. Return if the
	// name can't be found.
	char *loop_var_name = NULL;
	if (parent_expr->left->type == EXPR_SYMBOL)
		loop_var_name = parent_expr->left->symbol_name->name;
	else if (parent_expr->right->type == EXPR_SYMBOL)
		loop_var_name = parent_expr->right->symbol_name->name;
	if (!loop_var_name)
		return;

	// Get the cond expr as a statement. The built in functions
	// contain null checks, so we just need to check stmt.
	struct statement *stmt;
	stmt = expr_get_parent_stmt(parent_expr);

	// Check that it is a while- or for-loop using an iterator.
	if (!stmt)
		return;
	if (stmt->type != STMT_ITERATOR)
		return;

	// Loop over each line of code in the loop body.
	struct statement *tmp;
	FOR_EACH_PTR(stmt->iterator_statement->stmts, tmp)
	{
		// We look for declarations coming from a double pointer deref.
		if (tmp->type == STMT_DECLARATION)
		{
			struct symbol *sym;

			// In the RHS of the declaration, we iterate over the symbols.
			FOR_EACH_PTR(tmp->declaration, sym)
			{
				if (sym->namespace == NS_SYMBOL)
				{
					// Detect if there is the first dereference. 42 is ASCII for *.
					if (sym->initializer->type == EXPR_PREOP)
					{
						if (sym->initializer->op == 42)
						{
							// Detect if there is the second deref.
							if (sym->initializer->unop->type == EXPR_PREOP)
							{
								if (sym->initializer->unop->op == 42)
								{
									sm_warning("found pointer double deref in while loop (assign to **): %s", expr_to_str(sym->initializer));
								}
							}
						}
					}
				}
			}
			END_FOR_EACH_PTR(sym);
		}
		else if (tmp->type == STMT_EXPRESSION)
		{
			// Look for assignment to loop variable.
			if (tmp->expression->type == EXPR_ASSIGNMENT)
			{
				// Check that assigned variable on the left is the loop var.
				if (tmp->expression->left->type == EXPR_SYMBOL)
				{
					if (strcmp(tmp->expression->left->symbol_name->name, loop_var_name) == 0)
					{
						// Loop var has been reassigned. Is a deref on the right?
						// This is characteristic of a linked list traversal.
						if (tmp->expression->right->type == EXPR_DEREF)
						{
							if (tmp->expression->right->deref->type == EXPR_PREOP)
							{
								// Is the deref from the loop var itself? This
								// looks for assignment to the 'next' pointer.
								if (tmp->expression->right->deref->unop->type == EXPR_SYMBOL)
								{
									if (strcmp(tmp->expression->right->deref->unop->symbol_name->name, loop_var_name) == 0)
									{
										sm_warning("found pointer chasing in loop variable (linked list traversal): %s", expr_to_str(tmp->expression));
									}
								}
							}
						}
					}
				}
			}

			// Naive string search for double deref.
			const char *expr_str = expr_to_str(tmp->expression);
			const char *result = strstr(expr_str, "**");
			if (result != NULL)
			{
				sm_warning("found pointer double deref in while loop (**): %s", expr_to_str(tmp->expression));
			}

			const char *pattern = "->[^,\\s]+->";
			regex_t regex;
			int reti = regcomp(&regex, pattern, REG_EXTENDED);
			if (reti)
			{
				continue;
			}

			reti = regexec(&regex, expr_str, 0, NULL, 0);
			if (!reti)
			{
				sm_warning("found pointer double deref in while loop (->->): %s", expr_to_str(tmp->expression));
			}

			regfree(&regex);
		}
	}
	END_FOR_EACH_PTR(tmp);
}

void check_do_while_loop_limit(int id)
{
	// Hook is called every time there is a conditional
	// statement in the source code.
	my_id = id;
	add_hook(&double_deref_in_iter, CONDITION_HOOK);
}
