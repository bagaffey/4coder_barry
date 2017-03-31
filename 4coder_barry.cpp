#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "4coder_default_include.cpp"
#include "4coder_jump_parsing.cpp"

#if !defined(Assert)
#define Assert assert
#endif

#if !defined(internal)
#define internal static
#endif

struct Parsed_Error
{
  int exists;
  
  String target_file_name;
  int target_line_number;
  int target_column_number;
  
  int source_buffer_id;
  int source_position;
};

static bool GlobalEditMode;
static char *GlobalCompilationBufferName = "*compilation*";

static char BuildDirectory[4096] = "./";

enum token_type
{
  Token_Unknown,
  Token_OpenParen,
  Token_CloseParen,
  Token_Asterisk,
  Token_Minus,
  Token_Plus,
  Token_ForwardSlash,
  Token_Percent,
  Token_Colon,
  Token_Number,
  Token_Comma,
  Token_EndOfStream,
};

struct token
{
  token_type Type;
  size_t TextLength;
  char *Text;
};

struct tokenizer
{
  char *At;
};

inline bool
IsEndOfLine(char C)
{
  bool Result = ((C == '\n') ||
                 (C == '\r'));
  return(Result);
}

inline bool
IsWhitespace(char C)
{
  bool Result = ((C == ' ') ||
                 (C == '\t') ||
                 (C == '\v') ||
                 (C == '\f') ||
                 IsEndOfLine(C));
  return(Result);
}

inline bool
IsAlpha(char C)
{
    bool Result = (((C >= 'a') && (C <= 'z')) ||
                   ((C >= 'A') && (C <= 'Z')));
    return(Result);
}

inline bool
IsNumeric(char C)
{
    bool Result = ((C >= '0') && (C <= '9'));
    return(Result);
}

static void
EatAllWhitespace(tokenizer *Tokenizer)
{
    for(;;)
    {
        if(IsWhitespace(Tokenizer->At[0]))
        {
            ++Tokenizer->At;
        }
        else if((Tokenizer->At[0] == '/') &&
                (Tokenizer->At[1] == '/'))
        {
            Tokenizer->At += 2;
            while(Tokenizer->At[0] && !IsEndOfLine(Tokenizer->At[0]))
            {
                ++Tokenizer->At;
            }
        }
        else if((Tokenizer->At[0] == '/') &&
                (Tokenizer->At[1] == '*'))
        {
            Tokenizer->At += 2;
            while(Tokenizer->At[0] &&
                  !((Tokenizer->At[0] == '*') &&
                    (Tokenizer->At[1] == '/')))
            {
                ++Tokenizer->At;
            }
            
            if(Tokenizer->At[0] == '*')
            {
                Tokenizer->At += 2;
            }
        }
        else
        {
            break;
        }
    }
}

static token
GetToken(tokenizer *Tokenizer)
{
    EatAllWhitespace(Tokenizer);
    
    token Token = {};
    Token.TextLength = 1;
    Token.Text = Tokenizer->At;
    char C = Tokenizer->At[0];
    ++Tokenizer->At;
    switch(C)
    {
        case 0: {--Tokenizer->At; Token.Type = Token_EndOfStream;} break;
        
        case '(': {Token.Type = Token_OpenParen;} break;
        case ')': {Token.Type = Token_CloseParen;} break;
        case '*': {Token.Type = Token_Asterisk;} break;
        case '-': {Token.Type = Token_Minus;} break;
        case '+': {Token.Type = Token_Plus;} break;
        case '/': {Token.Type = Token_ForwardSlash;} break;
        case '%': {Token.Type = Token_Percent;} break;
        case ':': {Token.Type = Token_Colon;} break;
        case ',': {Token.Type = Token_Comma;} break;
        
        default:
        {
            if(IsNumeric(C))
            {
                Token.Type = Token_Number;
                while(IsNumeric(Tokenizer->At[0]) ||
                      (Tokenizer->At[0] == '.') ||
                      (Tokenizer->At[0] == 'f'))
                {
                    ++Tokenizer->At;
                    Token.TextLength = Tokenizer->At - Token.Text;
                }
            }
            else
            {
                Token.Type = Token_Unknown;
            }
        } break;
    }
    
    return(Token);
}

static token
PeekToken(tokenizer *Tokenizer)
{
    tokenizer Tokenizer2 = *Tokenizer;
    token Result = GetToken(&Tokenizer2);
    return(Result);
}

inline bool
IsH(String extension)
{
    bool Result = (match(extension, make_lit_string("h")) ||
                   match(extension, make_lit_string("hpp")) ||
                   match(extension, make_lit_string("hin")));
    return(Result);
}

inline bool
IsCPP(String extension)
{
    bool Result = (match(extension, make_lit_string("c")) ||
                   match(extension, make_lit_string("cpp")) ||
                   match(extension, make_lit_string("cin")));
    return(Result);
}

inline bool
IsINL(String extension)
{
    bool Result = (match(extension, make_lit_string("inl")) != 0);
    return(Result);
}

inline bool
IsCode(String extension)
{
    bool Result = (IsH(extension) || IsCPP(extension) || IsINL(extension));
    return(Result);
}
