#include "command.h"
#include "video.h"
#include "keyboard.h"
#include "string.h"

cmd_args get_args(char *str_args) {
    u8 in_quotes = 0;
    int amount_args = 0;

   if (str_args[0] == '\0') {
        cmd_args empty_args = {0};
        empty_args.amount = 0;
        return empty_args;
    }
    
    // Запоминаем, где начинается первый аргумент
    char *arg_start = str_args; 
    cmd_args argss;

    for (int i = 0; str_args[i]; i++) {
        if (str_args[i] == '"') in_quotes = !in_quotes;
        
        if (str_args[i] == ' ' && !in_quotes) {
            str_args[i] = 0; // Превращаем пробел в конец строки
            
            // Копируем текущий аргумент своими силами (вручную)
            if (amount_args < 64) {
                int src_idx = 0;
                // Копируем пока не встретим 0, но не больше 15 символов (чтобы оставить место для \0)
                while (arg_start[src_idx] != 0 && src_idx < 63) {
                    argss.args[amount_args][src_idx] = arg_start[src_idx];
                    src_idx++;
                }
                argss.args[amount_args][src_idx] = 0; // Закрываем строку нулем
                amount_args++;
            }
            
            // Следующий аргумент начнется сразу за текущим зануленным пробелом
            arg_start = &str_args[i + 1];
        }
    }
    
    // Копируем последний аргумент, так как за ним нет пробела
    if (amount_args < 64 && *arg_start != 0) {
        int src_idx = 0;
        while (arg_start[src_idx] != 0 && src_idx < 63) {
            argss.args[amount_args][src_idx] = arg_start[src_idx];
            src_idx++;
        }
        argss.args[amount_args][src_idx] = 0;
        amount_args++;
    }

    argss.amount = amount_args;
    return argss; // Возвращаем заполненную структуру
}