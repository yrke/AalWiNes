%{ /* -*- C++ -*- */
/* 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Copyright Peter G. Jensen
 */
    
//
// Created by Peter G. Jensen on 10/15/18.
//

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <sstream>
#include <fstream>
#include "generated_QueryParser.hh"
#include "QueryBuilder.h"
#include "parsererrors.h"
#include "Scanner.h"
#undef  YY_DECL
#define YY_DECL int aalwines::Scanner::yylex()
using token = aalwines::Parser::token;
%}

%option noyywrap nounput batch noinput c++
%option yyclass="aalwines::Scanner"
%x comment S_STRING

id    [a-zA-Z\_\-][a-zA-Z0-9\_\-]*
hex   [0-9a-fA-F]
int   [0-9]+
float {int}*\.{int}+
blank [ \t]
literal  \"(\\.|[^\\"])*\"

%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  builder._location.columns (yyleng);
  # define MAX_INCLUDE_DEPTH 10
  size_t depth = 0;
%}

%%

%{
  // Code run each time yylex is called.
  builder._location.step();
%}

{blank}+   builder._location.step ();
[\n\r]+    builder._location.lines(yyleng); builder._location.step();
"//".*     ;

"/*"                    BEGIN(comment);

<comment>[^*\n]*        /* eat anything that's not a '*' */
<comment>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<comment>"*"+"/"        BEGIN(INITIAL);
<comment><<EOF>>        {
                            aalwines::Parser::syntax_error
                                       (builder._location, "Unterminated multiline comment: " + std::string(yytext));
                        }

"("     return token::LPAREN;
")"     return token::RPAREN;
"["     return token::LSQBRCKT;
"]"     return token::RSQBRCKT;
"&"     return token::AND;
"|"     return token::OR;
"+"     return token::PLUS;
"*"     return token::STAR;
"∗"     return token::STAR;
"?"     return token::QUESTION;
"."     return token::DOT;
"·"     return token::DOT;
"<"     return token::LT;
">"     return token::GT;
","     return token::COMMA;
"^"     return token::HAT;
"#"     return token::HASH;
":"     return token::COLON;
"OVER"  return token::OVER;
"UNDER" return token::UNDER;
"DUAL"  return token::DUAL;
"EXACT" return token::EXACT;
"⊙"     return token::ANYIP;
"@"     return token::ANYIP;
"ip"    return token::ANYIP;
"⊕"     return token::ANYMPLS;
"¤"     return token::ANYMPLS;
"$"     return token::STICKY;
"mpls"  return token::ANYMPLS;
"smpls" return token::ANYSTICKYMPLS;
"$mpls" return token::ANYSTICKYMPLS;

s[0-9]+ {
  long int n = strtol (yytext + 1, NULL, 10);
    if (! (INT_MIN <= n && n <= INT_MAX)) {
        last_string = yytext;
        return token::IDENTIFIER;
    }
    last_string = yytext;
    last_int = n;
    return token::SNUMBER;
}

{int}      {
  long int n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX))
    throw aalwines::syntax_error (builder._location, "integer is out of range: " + std::string(yytext));

  last_int = n;
  return token::NUMBER;
}

{hex}     {
  long int n = strtol (yytext, NULL, 16);
  if (! (INT_MIN <= n && n <= INT_MAX))
    throw aalwines::syntax_error (builder._location, "integer is out of range: " + std::string(yytext));

  last_int = n;
  return token::HEX;
}

{literal}   {
    last_string = yytext;
    return token::LITERAL;
}
{id} {
    last_string = yytext;
    return token::IDENTIFIER;
}

'[^'\\]*   { last_string = std::string(yytext + 1, yyleng - 1);
             BEGIN(S_STRING);
           }
<S_STRING>{
  [^'\\]+  { last_string.append(yytext, yyleng); }
  \\('|\\) { last_string += yytext[1]; }
  '        { BEGIN(INITIAL); return token::STRINGLIT; }
}
