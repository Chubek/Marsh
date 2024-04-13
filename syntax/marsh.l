%option noyywrap

%{
#include <stdio.h>
%}

%option noyywrap
%option stack


ident	[a-zA-Z_][a-zA-Z0-9_]*
nnum 	[1-9][0-9]*



%x COMMENT SQUOTE DQUOTE BACKTICK PARAM EXPN

%%



"if"                    { return IF; }
"then"                  { return THEN; }
"else"                  { return ELSE; }
"fi"                    { return FI; }
"case"                  { return CASE; }
"esac"                  { return ESAC; }
"for"                   { return FOR; }
"while"                 { return WHILE; }
"do"                    { return DO; }
"done"                  { return DONE; }
"in"                    { return IN; }

"&&"                    { return AND_IF; }
"||"                    { return OR_IF; }


"$"			{ 
				yy_push_state(PARAM); 
				return *yytext; 
			}

<PARAM>"{"		{ yy_push_state(EXPAND); return EXPAND_INIT; }

<PARAM,EXPAND>{

[ \t]+			{
				if (yy_top_state() == EXPAND)
					scan_error("Illegal space during parameter expansion");
				else
					yy_pop_state();

			}

[\r\n]+			scan_error("Illegal newline during parameter expansion");

{ident}			{
				yylval.str_val = create_string(yytext, yyleng);
				if (yy_top_state() == EXPAND)
					return EXPAND_ID;
				else
					return PARAM_ID;

			}

{nnum}+			{
				yylval.int_val = strtoll(yytext, NULL, 10);
				if (yy_top_state() == EXPAND)
					return EXPAND_DIGIT;
				else
					return PARAM_DIGIT;
			}


":?"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_COLON_QMARK;
				else
					scan_error("Illegal token `%s`", yytext);

			}


":+"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_COLON_PLUS;
				else
					scan_error("Illegal token `%s`", yytext);

			}


":="			{
				if (yy_top_state() == EXPAND)
					return EXPAND_COLON_EQ;
				else
					scan_error("Illegal token `%s`", yytext);

			}

":-"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_COLON_DASH;
				else
					scan_error("Illegal token `%s`", yytext);

			}





"*"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_ASTERISK;
				else
					return PARAM_ASTERISK;
			}

"@"			{
				if (yy_top_state() == EXPAND)	
					return EXPAND_ATSIGN;
				else
					return PARAM_ATSIGN;

			}

"#"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_POUND;
				else
					return PARAM_POUND;

			}

"?"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_QMARK;
				else
					return PARAM_QMARK;

			}

"-"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_DASH;
				else
					return PARAM_DASH;

			}

"$"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_DOLLAR;
				else
					return PARAM_DOLLAR;

			}

"!"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_EXCLAIM;
				else
					return PARAM_EXCLAIM;

			}

"0"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_ZERO;
				else
					return PARAM_ZERO;

			}

"_"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_UNDERLINE;
				else
					return PARAM_UNDERLINE;

			}

"+"			{
				if (yy_top_state() == EXPAND)
					return EXPAND_PLUS;
				scan_error("Illegal token %s", yytext);
			}

.+			{
				yylval.str_val = create_string(yytext, yyleng);
				if (yy_top_state() == EXPAND)
					return EXPAND_WORD;
				scan_error("Illegal token %s", yytext);

			}

}


