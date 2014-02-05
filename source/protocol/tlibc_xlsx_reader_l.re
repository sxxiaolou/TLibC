#include "tlibc/protocol/tlibc_xlsx_reader.h"
#include "tlibc_xlsx_reader_l.h"

#define YYGETCONDITION()  self->scanner.state
#define YYSETCONDITION(s) self->scanner.state = s
#define STATE(name)  yyc##name
#define BEGIN(state) YYSETCONDITION(STATE(state))
#define YYCURSOR self->scanner.cursor
#define YYLIMIT self->scanner.limit
#define YYMARKER self->scanner.marker
#define YYCTYPE char
/*!re2c
re2c:yyfill:enable   = 0;
*/

void xpos2pos(tlibc_xlsx_pos *self, const char* xpos)
{
	self->col = 0;
	while(*xpos >='A')
	{
		self->col *= 26;
		self->col += *xpos - 'A';
		++xpos;
	}

	self->row = 0;
	while(*xpos != 0)
	{
		self->row *= 10;
		self->row += *xpos - '0';
		++xpos;
	}
}

TLIBC_ERROR_CODE tlibc_xlsx_reader_loadsheet(tlibc_xlsx_reader_t *self, tuint32 bindinfo_row, tuint32 data_row)
{
	tlibc_xlsx_cell_s *cell = NULL;
	int is_sharedstring = FALSE;
	tlibc_xlsx_cell_s *current_row = NULL;

	self->cell_matrix = NULL;
	self->scanner.cursor = self->sheet_buff;
	self->scanner.limit = self->sheet_buff + self->sheet_buff_size;
	self->scanner.marker = self->scanner.cursor;
	self->scanner.state = yycINITIAL;
	self->cell_matrix = NULL;
	self->real_row_size = 0;
	self->bindinfo_row = NULL;
	self->data_row = NULL;

restart:
	if(self->scanner.cursor >= self->scanner.limit)
	{
		if((self->bindinfo_row == NULL) || (self->data_row == NULL))
		{
			goto ERROR_RET;
		}
		return E_TLIBC_NOERROR;
	}
/*!re2c
<INITIAL>"<dimension ref=\""
{
	char *size_min = YYCURSOR;
	char *size_max = NULL;

	
	while(*YYCURSOR != '"')
	{
		if(*YYCURSOR == ':')
		{
			*YYCURSOR = 0;
			++YYCURSOR;
			size_max = YYCURSOR;
		}
		else
		{
			++YYCURSOR;
		}
	}
	*YYCURSOR = 0;
	++YYCURSOR;

	xpos2pos(&self->cell_min_pos, size_min);
	xpos2pos(&self->cell_max_pos, size_max);
	self->cell_row_size = (self->cell_max_pos.row - self->cell_min_pos.row + 1);
	self->cell_col_size = (self->cell_max_pos.col - self->cell_min_pos.col + 1);
	self->cell_matrix = malloc(sizeof(tlibc_xlsx_cell_s) * self->cell_row_size * self->cell_col_size);
	if(self->cell_matrix == NULL)
	{
		goto ERROR_RET;
	}
	goto restart;
}
<INITIAL>"<sheetData>"				{ BEGIN(IN_SHEETDATA);goto restart;	}
<IN_SHEETDATA>"<row r=\""
{
	tuint32 i;
	const char *r = YYCURSOR;
	tuint32 row;
	int is_single = FALSE;
	while(*YYCURSOR != '"')
	{
		++YYCURSOR;
	}
	*YYCURSOR = 0;
	errno = 0;
	row = strtoul(r, NULL, 10);
	if(errno != 0)
	{
		goto ERROR_RET;
	}
	
	while(*YYCURSOR != '>')
	{
		if(*YYCURSOR == '/')
		{
			is_single = TRUE;
		}
		++YYCURSOR;
	}
	++YYCURSOR;
	if(is_single)
	{
		goto restart;
	}

	current_row = self->cell_matrix + self->real_row_size;	
	for(i = 0; i < self->cell_col_size; ++i)
	{
		current_row[i].empty = TRUE;
	}

	if(row == bindinfo_row)
	{
		self->bindinfo_row = current_row;
	}
	else if(row == data_row)
	{
		self->data_row = current_row;
		self->data_real_row_index = self->real_row_size;
	}
	BEGIN(IN_ROW);
	++self->real_row_size;
	goto restart;
}
<IN_ROW>"<c"
{
	cell = NULL;
	is_sharedstring = FALSE;

	BEGIN(IN_COL);	
	goto restart;
}
<IN_COL>"r=\""
{
	const char* xpos = YYCURSOR;
	tlibc_xlsx_pos pos;
	while(*YYCURSOR != '"')
	{
		++YYCURSOR;
	}
	*YYCURSOR = 0;
	++YYCURSOR;

	xpos2pos(&pos, xpos);
	cell = current_row + (pos.col - self->cell_min_pos.col);
	cell->xpos = xpos;
		
	goto restart;
}
<IN_COL>"t=\""
{
	if((*YYCURSOR == 's') && (*(YYCURSOR + 1) == '"'))
	{
		is_sharedstring = TRUE;
	}	
	
	while(*YYCURSOR != '>')
	{
		++YYCURSOR;
	}
	++YYCURSOR;
}
<IN_COL>"/>"
{
	BEGIN(IN_ROW);
	goto restart;
}
<IN_COL>"</c>"
{
	if(is_sharedstring)
	{
		tuint32 string_index;
		errno = 0;
		string_index = strtoul(cell->val_begin, NULL, 10);
		if(errno != 0)
		{
			goto ERROR_RET;
		}
		cell->val_begin = self->sharedstring_begin_list[string_index];
		cell->val_end = self->sharedstring_end_list[string_index];
	}
	BEGIN(IN_ROW);
	goto restart;
}
<IN_COL>"<v>"						{ cell->val_begin = YYCURSOR; goto restart;}
<IN_COL>"</v>"						{ cell->val_end = YYCURSOR - 4; *(YYCURSOR - 4)= 0; goto restart;}
<IN_ROW>"</row>"					{ BEGIN(IN_SHEETDATA); goto restart; }
<IN_SHEETDATA>"</sheetData>"		{ BEGIN(INITIAL); goto restart;		}
<INITIAL>"</sheetData>"				{ BEGIN(INITIAL);goto restart;		}
<*>[^]								{ goto restart;}
*/
ERROR_RET:
	if(self->cell_matrix != NULL)
	{
		free(self->cell_matrix);
	}
	return E_TLIBC_ERROR;
}
