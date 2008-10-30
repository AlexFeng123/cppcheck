/*
 * c++check - c/c++ syntax checking
 * Copyright (C) 2007 Daniel Marjamäki
 *
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/
 */


//---------------------------------------------------------------------------
#include "tokenize.h"
#include "CommonCheck.h"    // <- IsName
//---------------------------------------------------------------------------

#include <locale>
#include <fstream>

#include <map>
#include <string>
#include <cstring>

#include <stdlib.h>     // <- strtoul
#include <stdio.h>

#ifdef __BORLANDC__
#include <ctype.h>
#include <mem.h>
#endif

#ifndef _MSC_VER
#define _strdup(str) strdup(str)
#endif

//---------------------------------------------------------------------------

// Helper functions..

static void Define(const char Name[], const char Value[]);

static void addtoken(const char str[], const unsigned int lineno, const unsigned int fileno);

static void combine_2tokens(TOKEN *tok, const char str1[], const char str2[]);

static void DeleteNextToken(TOKEN *tok);

static TOKEN *_gettok(TOKEN *tok, int index)
{
    while (tok && index>0)
    {
        tok = tok->next;
        index--;
    }
    return tok;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------

std::vector<std::string> Files;
struct TOKEN *tokens, *tokens_back;

//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Defined symbols.
// "#define abc 123" will create a defined symbol "abc" with the value 123
//---------------------------------------------------------------------------

struct DefineSymbol
{
    char *name;
    char *value;
    struct DefineSymbol *next;
};
static struct DefineSymbol * dsymlist;

static void Define(const char Name[], const char Value[])
{
    if (!(Name && Name[0]))
        return;

    if (!(Value && Value[0]))
        return;

    // Is 'Value' a decimal value..
    bool dec = true, hex = true;
    for (int i = 0; Value[i]; i++)
    {
        if ( ! isdigit(Value[i]) )
            dec = false;

        if ( ! isxdigit(Value[i]) && (!(i==1 && Value[i]=='x')))
            hex = false;
    }

    if (!dec && !hex)
        return;

    char *strValue = _strdup(Value);

    if (!dec && hex)
    {
		// Convert Value from hexadecimal to decimal
		unsigned long value;
		std::istringstream istr(Value+2);
		istr >> std::hex >> value;
		std::ostringstream ostr;
		ostr << value;
        free(strValue);
        strValue = _strdup(ostr.str().c_str());
    }

    DefineSymbol *NewSym = new DefineSymbol;
    memset(NewSym, 0, sizeof(DefineSymbol));
    NewSym->name = _strdup(Name);
    NewSym->value = strValue;
    NewSym->next = dsymlist;
    dsymlist = NewSym;
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
// addtoken
// add a token. Used by 'Tokenizer'
//---------------------------------------------------------------------------

static void addtoken(const char str[], const unsigned int lineno, const unsigned int fileno)
{
    if (str[0] == 0)
        return;

    // Replace hexadecimal value with decimal
	std::ostringstream str2;
	if (strncmp(str,"0x",2)==0)
    {
        str2 << strtoul(str+2, NULL, 16);
    }
	else
	{
		str2 << str;
	}

    TOKEN *newtoken  = new TOKEN;
    memset(newtoken, 0, sizeof(TOKEN));
    newtoken->str    = _strdup(str2.str().c_str());
    newtoken->linenr = lineno;
    newtoken->FileIndex = fileno;
    if (tokens_back)
    {
        tokens_back->next = newtoken;
        tokens_back = newtoken;
    }
    else
    {
        tokens = tokens_back = newtoken;
    }

    // Check if str is defined..
    for (DefineSymbol *sym = dsymlist; sym; sym = sym->next)
    {
        if (strcmp(str,sym->name)==0)
        {
            free(newtoken->str);
            newtoken->str = _strdup(sym->value);
            break;
        }
    }
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
// combine_2tokens
// Combine two tokens that belong to each other. Ex: "<" and "=" may become "<="
//---------------------------------------------------------------------------

static void combine_2tokens(TOKEN *tok, const char str1[], const char str2[])
{
    if (!(tok && tok->next))
        return;
    if (strcmp(tok->str,str1) || strcmp(tok->next->str,str2))
        return;

    free(tok->str);
	std::string newstr(std::string(str1) + std::string(str2));
	tok->str = _strdup( newstr.c_str() );

    DeleteNextToken(tok);
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// SizeOfType - gives the size of a type
//---------------------------------------------------------------------------

std::map<std::string, unsigned int> TypeSize;

int SizeOfType(const char type[])
{
    if (!type)
        return 0;

    return TypeSize[type];
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// DeleteNextToken. Unlink and delete next token.
//---------------------------------------------------------------------------

static void DeleteNextToken(TOKEN *tok)
{
    TOKEN *next = tok->next;
    tok->next = next->next;
    free(next->str);
    delete next;
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
// InsertTokens - Copy and insert tokens
//---------------------------------------------------------------------------

static void InsertTokens(TOKEN *dest, TOKEN *src, unsigned int n)
{
    while (n > 0)
    {
        TOKEN *NewToken = new TOKEN;
        NewToken->FileIndex = src->FileIndex;
        NewToken->linenr = src->linenr;
        NewToken->str = _strdup(src->str);

        NewToken->next = dest->next;
        dest->next = NewToken;

        dest = dest->next;
        src  = src->next;
        n--;
    }
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// Tokenize - tokenizes a given file.
//---------------------------------------------------------------------------

void Tokenize(std::istream &code, const char FileName[])
{
    // Has this file been tokenized already?
    for (unsigned int i = 0; i < Files.size(); i++)
    {
        if ( SameFileName( Files[i].c_str(), FileName ) )
            return;
    }

    // The "Files" vector remembers what files have been tokenized..
    Files.push_back(FileName);

    // Tokenize the file..
    TokenizeCode( code, Files.size() - 1 );
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// Tokenize - tokenizes input stream
//---------------------------------------------------------------------------

void TokenizeCode(std::istream &code, const unsigned int FileIndex)
{
    // Tokenize the file.
    unsigned int lineno = 1;
    char CurrentToken[1000] = {0};
    char *pToken = CurrentToken;
    for (char ch = (char)code.get(); code.good(); ch = (char)code.get())
    {
		// Todo
		if ( ch < 0 )
			continue;

        // Preprocessor stuff?
        if (ch == '#' && !CurrentToken[0])
        {
            std::string line;
            getline(code,line);
            line = "#" + line;
            if (strncmp(line.c_str(),"#include",8)==0 &&
                line.find("\"") != std::string::npos)
            {
                // Extract the filename
                line.erase(0, line.find("\"")+1);
                line.erase(line.find("\""));

                // Relative path..
                if (Files.back().find_first_of("\\/") != std::string::npos)
                {
                    std::string path = Files.back();
                    path.erase( 1 + path.find_last_of("\\/") );
                    line = path + line;
                }

                addtoken("#include", lineno, FileIndex);
                addtoken(line.c_str(), lineno, FileIndex);

                std::ifstream fin( line.c_str() );
                Tokenize(fin, line.c_str());
            }

            else if (strncmp(line.c_str(), "#define", 7) == 0)
            {
                char *strId = NULL;
                enum {Space1, Id, Space2, Value} State;
                State = Space1;
                for (unsigned int i = 8; i < line.length(); i++)
                {
                    if (State==Space1 || State==Space2)
                    {
                        if (isspace(line[i]))
                            continue;
                        State = (State==Space1) ? Id : Value;
                    }

                    else if (State==Id)
                    {
                        if ( isspace( line[i] ) )
                        {
                            strId = _strdup(CurrentToken);
                            memset(CurrentToken, 0, sizeof(CurrentToken));
                            pToken = CurrentToken;
                            State = Space2;
                            continue;
                        }
                        else if ( ! isalnum(line[i]) )
                        {
                            break;
                        }
                    }

                    *pToken = line[i];
                    pToken++;
                }

                if (State==Value)
                {
                    addtoken("def", lineno, FileIndex);
                    addtoken(strId, lineno, FileIndex);
                    addtoken(";", lineno, FileIndex);
                    Define(strId, CurrentToken);
                }

                pToken = CurrentToken;
                memset(CurrentToken, 0, sizeof(CurrentToken));
                free(strId);
            }

            else
            {
                addtoken("#", lineno, FileIndex);
                addtoken(";", lineno, FileIndex);
            }

            lineno++;
            continue;
        }

        if (ch == '\n')
        {
            // Add current token..
            addtoken(CurrentToken, lineno++, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;
            continue;
        }

        // Comments..
        if (ch == '/' && code.good())
        {
            bool newstatement = bool( strchr(";{}", CurrentToken[0]) != NULL );

            // Add current token..
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;

            // Read next character..
            ch = (char)code.get();

            // If '//'..
            if (ch == '/')
            {
                std::string comment;
                getline( code, comment );   // Parse in the whole comment

                // If the comment says something like "fred is deleted" then generate appropriate tokens for that
                comment = comment + " ";
                if ( newstatement && comment.find(" deleted ")!=std::string::npos )
                {
                    // delete
                    addtoken( "delete", lineno, FileIndex );

                    // fred
                    std::string::size_type pos1 = comment.find_first_not_of(" \t");
                    std::string::size_type pos2 = comment.find(" ", pos1);
                    std::string firstWord = comment.substr( pos1, pos2-pos1 );
                    addtoken( firstWord.c_str(), lineno, FileIndex );

                    // ;
                    addtoken( ";", lineno, FileIndex );
                }

                lineno++;
                continue;
            }

            // If '/*'..
            if (ch == '*')
            {
                char chPrev;
                ch = chPrev = 'A';
                while (code.good() && (chPrev!='*' || ch!='/'))
                {
                    chPrev = ch;
                    ch = (char)code.get();
                    if (ch == '\n')
                        lineno++;
                }
                continue;
            }

            // Not a comment.. add token..
            addtoken("/", lineno, FileIndex);
        }

        // char..
        if (ch == '\'')
        {
            // Add previous token
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));

            // Read this ..
            CurrentToken[0] = ch;
            CurrentToken[1] = (char)code.get();
            CurrentToken[2] = (char)code.get();
            if (CurrentToken[1] == '\\')
                CurrentToken[3] = (char)code.get();

            // Add token and start on next..
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;

            continue;
        }

        // String..
        if (ch == '\"')
        {
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;
            bool special = false;
            char c = ch;
            do
            {
                // Append token..
                if ( pToken < &CurrentToken[sizeof(CurrentToken)-10] )
                {
                    *pToken = c;
                    pToken++;
                }

                // Special sequence '\.'
                if (special)
                    special = false;
                else
                    special = (c == '\\');

                // Get next character
                c = (char)code.get();
            }
            while (code.good() && (special || c != '\"'));
            *pToken = '\"';
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;
            continue;
        }

        if (strchr("+-*/%&|^?!=<>[](){};:,.",ch))
        {
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            CurrentToken[0] = ch;
            addtoken(CurrentToken, lineno, FileIndex);
            memset(CurrentToken, 0, sizeof(CurrentToken));
            pToken = CurrentToken;
            continue;
        }


        if (isspace(ch) || iscntrl(ch))
        {
            addtoken(CurrentToken, lineno, FileIndex);
            pToken = CurrentToken;
            memset(CurrentToken, 0, sizeof(CurrentToken));
            continue;
        }

        *pToken = ch;
        pToken++;
    }

    // Combine tokens..
    for (TOKEN *tok = tokens; tok && tok->next; tok = tok->next)
    {
        combine_2tokens(tok, "<", "<");
        combine_2tokens(tok, ">", ">");

        combine_2tokens(tok, "&", "&");
        combine_2tokens(tok, "|", "|");

        combine_2tokens(tok, "+", "=");
        combine_2tokens(tok, "-", "=");
        combine_2tokens(tok, "*", "=");
        combine_2tokens(tok, "/", "=");
        combine_2tokens(tok, "&", "=");
        combine_2tokens(tok, "|", "=");

        combine_2tokens(tok, "=", "=");
        combine_2tokens(tok, "!", "=");
        combine_2tokens(tok, "<", "=");
        combine_2tokens(tok, ">", "=");

        combine_2tokens(tok, ":", ":");
        combine_2tokens(tok, "-", ">");

        combine_2tokens(tok, "private", ":");
        combine_2tokens(tok, "protected", ":");
        combine_2tokens(tok, "public", ":");
    }

    // Replace "->" with "."
    for ( TOKEN *tok = tokens; tok; tok = tok->next )
    {
        if ( strcmp(tok->str, "->") == 0 )
        {
            tok->str[0] = '.';
			tok->str[1] = 0;
        }
    }

    // typedef..
    for ( TOKEN *tok = tokens; tok; tok = tok->next )
    {
        if (Match(tok, "typedef %type% %type% ;"))
        {
            const char *type1 = getstr(tok, 1);
            const char *type2 = getstr(tok, 2);
            for ( TOKEN *tok2 = tok; tok2; tok2 = tok2->next )
            {
                if (tok2->str!=type1 && tok2->str!=type2 && strcmp(tok2->str,type2)==0)
                {
                    free(tok2->str);
                    tok2->str = _strdup(type1);
                }
            }
        }

        else if (Match(tok, "typedef %type% %type% %type% ;"))
        {
            const char *type1 = getstr(tok, 1);
            const char *type2 = getstr(tok, 2);
            const char *type3 = getstr(tok, 3);

            TOKEN *tok2 = tok;
            while ( ! Match(tok2, ";") )
                tok2 = tok2->next;

            for ( ; tok2; tok2 = tok2->next )
            {
                if (tok2->str!=type3 && strcmp(tok2->str,type3)==0)
                {
                    free(tok2->str);
                    tok2->str = _strdup(type1);

                    TOKEN *newtok = new TOKEN;
                    newtok->str = _strdup(type2);
                    newtok->FileIndex = tok2->FileIndex;
                    newtok->linenr = tok2->linenr;
                    newtok->next = tok2->next;
                    tok2->next = newtok;
                    tok2 = newtok;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
// Simplify token list
//---------------------------------------------------------------------------

void SimplifyTokenList()
{

    // Remove the keyword 'unsigned'
    for ( TOKEN *tok = tokens; tok; tok = tok->next )
    {
        if (tok->next && strcmp(tok->next->str,"unsigned")==0)
        {
            DeleteNextToken( tok );
        }
    }

    // Replace constants..
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        if (Match(tok,"const %type% %var% = %num% ;"))
        {
            const char *sym = getstr(tok,2);
            const char *num = getstr(tok,4);

            for (TOKEN *tok2 = _gettok(tok,6); tok2; tok2 = tok2->next)
            {
                if (strcmp(tok2->str,sym) == 0)
                {
                    free(tok2->str);
                    tok2->str = _strdup(num);
                }
            }
        }
    }


    // Fill the map TypeSize..
    TypeSize.clear();
    TypeSize["char"] = sizeof(char);
    TypeSize["short"] = sizeof(short);
    TypeSize["int"] = sizeof(int);
    TypeSize["long"] = sizeof(long);
    TypeSize["float"] = sizeof(float);
    TypeSize["double"] = sizeof(double);
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        if (Match(tok,"class %var%"))
        {
            TypeSize[getstr(tok,1)] = 11;
        }

        else if (Match(tok, "struct %var%"))
        {
            TypeSize[getstr(tok,1)] = 13;
        }
    }


    // Replace 'sizeof(type)'..
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        if (strcmp(tok->str,"sizeof") != 0)
            continue;

        if (Match(tok, "sizeof ( %type% * )"))
        {
            free(tok->str);
			std::ostringstream str;
            // 'sizeof(type *)' has the same size as 'sizeof(char *)'
            str << sizeof(char *);
            tok->str = _strdup( str.str().c_str() );

            for (int i = 0; i < 4; i++)
            {
                DeleteNextToken(tok);
            }
        }

        else if (Match(tok, "sizeof ( %type% )"))
        {
            const char *type = getstr(tok, 2);
            int size = SizeOfType(type);
            if (size > 0)
            {
                free(tok->str);
				std::ostringstream str;
				str << size;
                tok->str = _strdup( str.str().c_str() );
                for (int i = 0; i < 3; i++)
                {
                    DeleteNextToken(tok);
                }
            }
        }
    }

    // Replace 'sizeof(var)'
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        // type array [ num ] ;
        if ( ! Match(tok, "%type% %var% [ %num% ] ;") )
            continue;

        int size = SizeOfType(tok->str);
        if (size <= 0)
            continue;

        const char *varname = getstr(tok, 1);
        int total_size = size * atoi( getstr(tok, 3) );

        // Replace 'sizeof(var)' with number
        int indentlevel = 0;
        for ( TOKEN *tok2 = _gettok(tok,5); tok2; tok2 = tok2->next )
        {
            if (tok2->str[0] == '{')
            {
                indentlevel++;
            }

            else if (tok2->str[0] == '}')
            {
                indentlevel--;
                if (indentlevel < 0)
                    break;
            }

            // Todo: Match varname directly
            else if (Match(tok2, "sizeof ( %var% )"))
            {
                if (strcmp(getstr(tok2,2), varname) == 0)
                {
                    free(tok2->str);
					std::ostringstream str;
                    str << total_size;
                    tok2->str = _strdup(str.str().c_str());
                    // Delete the other tokens..
                    for (int i = 0; i < 3; i++)
                    {
                        DeleteNextToken(tok2);
                    }
                }
            }
        }
    }




    // Simple calculations..

    bool done = false;
    while (!done)
    {
        done = true;

        for (TOKEN *tok = tokens; tok; tok = tok->next)
        {
            if (Match(tok->next, "* 1") || Match(tok->next, "1 *"))
            {
                for (int i = 0; i < 2; i++)
                    DeleteNextToken(tok);
                done = false;
            }

            // (1-2)
            if (strchr("[,(=<>",tok->str[0]) &&
                IsNumber(getstr(tok,1))   &&
                strchr("+-*/",*(getstr(tok,2))) &&
                IsNumber(getstr(tok,3))   &&
                strchr("],);=<>",*(getstr(tok,4))) )
            {
                int i1 = atoi(getstr(tok,1));
                int i2 = atoi(getstr(tok,3));
                if ( i2 == 0 && *(getstr(tok,2)) == '/' )
                {
                    continue;
                }

                switch (*(getstr(tok,2)))
                {
                    case '+': i1 += i2; break;
                    case '-': i1 -= i2; break;
                    case '*': i1 *= i2; break;
                    case '/': i1 /= i2; break;
                }
                tok = tok->next;
                free(tok->str);
				std::ostringstream str;
                str <<  i1;
                tok->str = _strdup(str.str().c_str());
                for (int i = 0; i < 2; i++)
                {
                    DeleteNextToken(tok);
                }

                done = false;
            }
        }
    }


    // Replace "*(str + num)" => "str[num]"
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        if ( ! strchr(";{}(=<>", tok->str[0]) )
            continue;

        TOKEN *next = tok->next;
        if ( ! next )
            break;

        if (Match(next, "* ( %var% + %num% )"))
        {
            const char *str[4] = {"var","[","num","]"};
            str[0] = getstr(tok,3);
            str[2] = getstr(tok,5);

            for (int i = 0; i < 4; i++)
            {
                tok = tok->next;
                free(tok->str);
                tok->str = _strdup(str[i]);
            }

            DeleteNextToken(tok);
            DeleteNextToken(tok);
        }
    }



    // Split up variable declarations if possible..
    for (TOKEN *tok = tokens; tok; tok = tok->next)
    {
        if ( ! strchr("{};", tok->str[0]) )
            continue;

        TOKEN *type0 = tok->next;
        if (!type0)
            break;
        if (strcmp(type0->str, "else") == 0)
            continue;

        TOKEN *tok2 = NULL;
        unsigned int typelen = 0;

        if ( Match(type0, "%type% %var% ,") )
        {
            tok2 = _gettok(type0, 2);    // The ',' token
            typelen = 1;
        }

        else if ( Match(type0, "%type% * %var% ,") )
        {
            tok2 = _gettok(type0, 3);    // The ',' token
            typelen = 1;
        }

        else if ( Match(type0, "%type% %var% [ %num% ] ,") )
        {
            tok2 = _gettok(type0, 5);    // The ',' token
            typelen = 1;
        }

        else if ( Match(type0, "%type% * %var% [ %num% ] ,") )
        {
            tok2 = _gettok(type0, 6);    // The ',' token
            typelen = 1;
        }

        else if ( Match(type0, "struct %type% %var% ,") )
        {
            tok2 = _gettok(type0, 3);
            typelen = 2;
        }

        else if ( Match(type0, "struct %type% * %var% ,") )
        {
            tok2 = _gettok(type0, 4);
            typelen = 2;
        }


        else if ( Match(type0, "%type% %var% =") )
        {
            tok2 = _gettok(type0, 2);
            typelen = 1;
        }

        else if ( Match(type0, "%type% * %var% =") )
        {
            tok2 = _gettok(type0, 3);
            typelen = 1;
        }

        else if ( Match(type0, "struct %type% * %var% =") )
        {
            tok2 = _gettok(type0, 4);
            typelen = 2;
        }

        if (tok2)
        {
            if (tok2->str[0] == ',')
            {
                free(tok2->str);
                tok2->str = _strdup(";");
                InsertTokens(tok2, type0, typelen);
            }

            else
            {
                TOKEN *eq = tok2;

                int parlevel = 0;
                while (tok2)
                {
                    if ( strchr("{(", tok2->str[0]) )
                    {
                        parlevel++;
                    }

                    else if ( strchr("})", tok2->str[0]) )
                    {
                        if (parlevel<0)
                            break;
                        parlevel--;
                    }

                    else if ( parlevel==0 && strchr(";,",tok2->str[0]) )
                    {
                        // "type var ="   =>   "type var; var ="
                        TOKEN *VarTok = _gettok(type0,typelen);
                        if (VarTok->str[0]=='*')
                            VarTok = VarTok->next;
                        InsertTokens(eq, VarTok, 2);
                        free(eq->str);
                        eq->str = _strdup(";");

                        // "= x, "   =>   "= x; type "
                        if (tok2->str[0] == ',')
                        {
                            free(tok2->str);
                            tok2->str = _strdup(";");
                            InsertTokens( tok2, type0, typelen );
                        }
                        break;
                    }

                    tok2 = tok2->next;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
// Helper functions for handling the tokens list
//---------------------------------------------------------------------------

const TOKEN *findtoken(const TOKEN *tok1, const char *tokenstr[])
{
    for (const TOKEN *ret = tok1; ret; ret = ret->next)
    {
        unsigned int i = 0;
        const TOKEN *tok = ret;
        while (tokenstr[i])
        {
            if (!tok)
                return NULL;
            if (*(tokenstr[i]) && strcmp(tokenstr[i],tok->str))
                break;
            tok = tok->next;
            i++;
        }
        if (!tokenstr[i])
            return ret;
    }
    return NULL;
}
//---------------------------------------------------------------------------

const TOKEN *gettok(const TOKEN *tok, int index)
{
    while (tok && index>0)
    {
        tok = tok->next;
        index--;
    }
    return tok;
}
//---------------------------------------------------------------------------

const char *getstr(const TOKEN *tok, int index)
{
    tok = gettok(tok, index);
    return tok ? tok->str : "";
}
//---------------------------------------------------------------------------



// Deallocate lists..
void DeallocateTokens()
{
    while (tokens)
    {
        TOKEN *next = tokens->next;
        free(tokens->str);
        delete tokens;
        tokens = next;
    }
    tokens_back = tokens;

    while (dsymlist)
    {
        struct DefineSymbol *next = dsymlist->next;
        free(dsymlist->name);
        free(dsymlist->value);
        delete dsymlist;
        dsymlist = next; 
    }
}




