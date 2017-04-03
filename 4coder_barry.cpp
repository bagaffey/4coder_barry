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

CUSTOM_COMMAND_SIG(casey_open_in_other)
{
    exec_command(app, change_active_panel);
    exec_command(app, cmdid_interactive_open);
}

CUSTOM_COMMAND_SIG(casey_clean_and_save)
{
    exec_command(app, clean_all_lines);
    exec_command(app, eol_nixify);
    exec_command(app, cmdid_save);
}

CUSTOM_COMMAND_SIG(casey_newline_and_indent)
{
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    if (buffer.lock_flags & AccessProtected)
    {
        exec_command(app, goto_jump_at_cursor);
    }
    else
    {
        exec_command(app, write_character);
        exec_command(app, auto_tab_line_at_cursor);
    }
}

CUSTOM_COMMAND_SIG(casey_open_file_other_window)
{
    exec_command(app, change_active_panel);
    exec_command(app, cmdid_interactive_open);
}

CUSTOM_COMMAND_SIG(casey_switch_buffer_other_window)
{
    exec_command(app, change_active_panel);
    exec_command(app, cmdid_interactive_switch_buffer);
}

internal void
DeleteAfterCommand(struct Application_Links *app, unsigned long long CommandID)
{
    unsigned int access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    
    int pos2 = view.cursor.pos;
    if (CommandID < cmdid_count)
    {
        exec_command(app, (Command_ID)CommandID);
    }
    else
    {
        exec_command(app, (Custom_Command_Function*)CommandID);
    }
    refresh_view(app, &view);
    int pos1 = view.cursor.pos;
    
    Range range = make_range(pos1, pos2);
    
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    buffer_replace_range(app, &buffer, range.min, range.max, 0, 0);
}

CUSTOM_COMMAND_SIG(casey_delete_token_left)
{
    DeleteAfterCommand(app, (unsigned long long)seek_white_or_token_left);
}

CUSTOM_COMMAND_SIG(casey_delete_token_right)
{
    DeleteAfterCommand(app, (unsigned long long)seek_white_or_token_right);
}

CUSTOM_COMMAND_SIG(casey_kill_to_end_of_line)
{
    unsigned int access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    
    int pos2 = view.cursor.pos;
    exec_command(app, seek_end_of_line);
    refresh_view(app, &view);
    int pos1 = view.cursor.pos;
    
    Range range = make_range(pos1, pos2);
    if (pos1 == pos2)
    {
        range.max += 1;
    }
    
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    buffer_replace_range(app, &buffer, range.min, range.max, 0, 0);
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(casey_paste_and_tab)
{
    exec_command(app, paste);
    exec_command(app, auto_tab_range);
}

CUSTOM_COMMAND_SIG(casey_seek_beginning_of_line_and_tab)
{
    exec_command(app, seek_beginning_of_line);
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(casey_seek_beginning_of_line)
{
    exec_command(app, auto_tab_line_at_cursor);
    exec_command(app, seek_beginning_of_line);
}

struct switch_to_result
{
    bool Switched;
    bool Loaded;
    View_Summary view;
    Buffer_Summary buffer;
};

inline void
SanitizeSlashes(String value)
{
    for (int At = 0;
         At < Value.size;
         ++At;)
    {
        if (Value.str[At] == '\\')
        {
            Value.str[At] = '/';
        }
    }
}

inline switch_to_result
SwitchToOrLoadFile(struct Application_Links *app, String FileName, bool CreateIfNotFound = false)
{
    switch_to_result Result = {};
    
    SanitizeSlashes(FileName);
    
    unsigned int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer_by_name(app, FileName.str, FileName.size, access);
    
    Result.view = view;
    Result.buffer = buffer;
    
    if (buffer.exists)
    {
        view_set_buffer(app, &view, buffer.buffer_id, 0);
        Result.Switched = true;
    }
    else
    {
        if (file_exists(app, FileName.str, FileName.size) || CreateIfNotFound)
        {
            view_open_file(app, &view, FileName.str, FileName.size, true);
            
            Result.buffer = get_buffer_by_name(app, FileName.str, FileName.size, access);
            Result.Loaded = true;
            Result.Switched = true;
        }
    }
    return(Result);
}

CUSTOM_COMMAND_SIG(casey_load_todo)
{
    String ToDoFileName = make_lit_string("todo.txt");
    SwitchToOrLoadFile(app, ToDoFileName, true);
}

CUSTOM_COMMAND_SIG(casey_build_search)
{
    int keep_going = 1;
    int old_size;
    
    String dir = make_string(app->memory, 0, app->memory_size);
    dir.size = directory_get_hot(app, dir.str, dir.memory_size);
    
    while (keep_going)
    {
        old_size = dir.size;
        append(&dir, "build.bat");
        
        if (file_exists(app, dir.str, dir.size)
        {
            dir.size = old_size;
            memcpy(BuildDirectory, dir.str, dir.size);
            BuildDirectory[dir.size] = 0;
            
            print_message(app, literal("Building with: "));
            print_message(app, BuildDirectory, dir.size);
            print_message(app, literal("build.bat\n"));
            
            return;
        }
        
        dir.size = old_size;
        
        if (directory_cd(app, dir.str, &dir.size, dir.memory_size, literal("..")) == 0)
        {
            keep_going = 0;
            print_message(app, literal("Did not find build.bat\n"));
        }
    }
}

CUSTOM_COMMAND_SIG(casey_find_corresponding_file)
{
    unsigned int access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    String extension = file_extension(make_string(buffer.file_name, buffer.file_name_len));
    if (extension.str)
    {
        char *HExtensions[] =
        {
            "hpp",
            "hin",
            "h",
        };
        
        char *CExtensions[] =
        {
            "c",
            "cpp",
            "cin",
        };
        
        int ExtensionCount = 0;
        char **Extensions = 0;
        if(IsH(extension))
        {
            ExtensionCount = ArrayCount(CExtensions);
            Extensions = CExtensions;
        }
        else if(IsCPP(extension) || IsINL(extension))
        {
            ExtensionCount = ArrayCount(HExtensions);
            Extensions = HExtensions;
        }
        
        int MaxExtensionLength = 3;
        int Space = (int)(buffer.file_name_len + MaxExtensionLength);
        String FileNameStem = make_string(buffer.file_name, (int)(extension.str - buffer.file_name), 0);
        String TestFileName = make_string(app->memory, 0, Space);
        for (int ExtensionIndex = 0;
             ExtensionCount;
             ++ExtensionIndex)
        {
            TestFileName.size = 0;
            append(&TestFileName, FileNameStem);
            append(&TestFileName, Extensions[ExtensionIndex]);
            
            if (SwitchToOrLoadFile(app, TestFileName, ((ExtensionIndex + 1) == ExtensionCount)).Switched)
            {
                break;
            }
        }
    }
}

CUSTOM_COMMAND_SIG(casey_find_corresponding_file_other_window)
{
    unsigned int access = AccessProtected;
    View_Summary old_view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, old_view.buffer_id, access);
    
    exec_command(app, change_active_panel);
    View_Summary new_view = get_active_view(app, AccessAll);
    view_set_buffer(app, &new_view, buffer_buffer_id, 0);
}

CUSTOM_COMMAND_SIG(casey_save_and_make_without_asking)
{
    exec_command(app, change_active_panel);
    
    Buffer_Summary buffer = {};
    
    unsigned int access = AccessAll;
    for (buffer = get_buffer_first(app, access);
         buffer.exists;
         get_buffer_next(app, &buffer, access))
    {
        save_buffer(app, &buffer, buffer.file_name, buffer.file_name_len, 0);
    }
    
    int size = app->memory_size / 2;
    String dir = make_string(app->memory, 0, size);
    String command = make_string((char*)app->memory + size, 0, size);
    
    append(&dir, BuildDirectory);
    for (int At = 0;
         At < dir.size;
         ++At)
    {
        if (dir.str[At] == '/')
        {
            dir.str[At] = '\\';
        }
    }
    
    append(&command, dir);
    
    if (append(&command, "build.bat"))
    {
        unsigned int access = AccessAll;
        View_Summary view = get_active_view(app, access);
        char *BufferName = GlobalCompilationBufferName;
        int BufferNameLength = (int) strlen(GlobalCompilationBufferName);
        exec_system_command(app, &view,
                            buffer_identifier(BufferName, BufferNameLength),
                            dir.str, dir.size,
                            command.str, command.size
                            CLI_OverlapWithConflict);
        lock_jump_buffer(BufferName, BufferNameLength);
    }
    
    exec_command(app, change_active_panel);
    prev_location = null_location;
}
