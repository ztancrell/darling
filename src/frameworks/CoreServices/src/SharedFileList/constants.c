/*
 This file is part of Darling.

 Copyright (C) 2025 Darling Developers

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SharedFileList/SharedFileList.h>

struct OpaqueLSSharedFileListItemRef {};
static struct OpaqueLSSharedFileListItemRef _insertItemLast = { };

LSSharedFileListItemRef kLSSharedFileListItemLast = &_insertItemLast;

/* ---- Darling: local discovery placeholders, NOT implementations ----
 * Each logs its own name when called and returns zero, so one run shows which of these an
 * application actually reaches. Replace each with a real implementation, or remove it. */
#include <stdio.h>
#ifndef DARLING_UNIMPLEMENTED
#define DARLING_UNIMPLEMENTED(sym) \
	__attribute__((visibility("default"))) long darling_unimplemented_##sym(void) __asm__("_" #sym); \
	long darling_unimplemented_##sym(void) { fprintf(stderr, "DARLING unimplemented function called: " #sym "\n"); return 0; }
#endif
DARLING_UNIMPLEMENTED(LSSharedFileListCopySnapshot)
DARLING_UNIMPLEMENTED(LSSharedFileListCreate)
DARLING_UNIMPLEMENTED(LSSharedFileListInsertItemURL)
DARLING_UNIMPLEMENTED(LSSharedFileListItemCopyProperty)
DARLING_UNIMPLEMENTED(LSSharedFileListItemCopyResolvedURL)
DARLING_UNIMPLEMENTED(LSSharedFileListItemRemove)
