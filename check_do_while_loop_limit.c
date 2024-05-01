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

static int my_id;

static void double_deref_in_iter(struct expression *expr)
{
	expr = strip_expr(expr);

	// Get the complete conditional expression, not just
	// the variable it is conditioned on.
	struct expression *parent_expr;
	parent_expr = expr_get_parent_expr(expr);
	// printf("%s\n", expr_to_str(parent_expr));

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
	FOR_EACH_PTR(stmt->iterator_statement->stmts, tmp) {
		// We look for declarations coming from a double pointer deref.
		if (tmp->type == STMT_DECLARATION) {
			struct symbol *sym;

			// In the RHS of the declaration, we iterate over the symbols.
			FOR_EACH_PTR(tmp->declaration, sym) {
				if (sym->namespace == NS_SYMBOL) {
					// Detect if there is the first dereference. 42 is ASCII for *.
					if (sym->initializer->type == EXPR_PREOP) {
						if (sym->initializer->op == 42) {
							// Detect if there is the second deref.
							if (sym->initializer->unop->type == EXPR_PREOP) {
								if (sym->initializer->unop->op == 42) {
									sm_warning("found double pointer deref in iterating loop: %s", expr_to_str(sym->initializer));
								}
							}
						}
						
					}
				}
			} END_FOR_EACH_PTR(sym);
		}
		else if (tmp->type == STMT_EXPRESSION) {
			// printf("expression: %s\n", expr_to_str(tmp->expression));
			// printf("op: %c\n", tmp->expression->op);
		}
	} END_FOR_EACH_PTR(tmp);
}

void check_do_while_loop_limit(int id)
{
	// Hook is called every time there is a conditional
	// statement in the source code.
	my_id = id;
	add_hook(&double_deref_in_iter, CONDITION_HOOK);
}
