/**
 @file recovery.c Provides recovery functions.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "recovery.h"


/** 
 * Function opens the recovery binary file and executes all commands that were
 * saved inside the redo_log structure
 * @author Dražen Bandić, update by Tomislav Turek
 * @brief Reads binary file where last commands were saved, and executes them
 * @param fileName - name of the archive log
 * @return no value
 */
 void AK_recover_archive_log(char* fileName){
    FILE * fp;
    AK_PRO;
    
    printf("AK_recover_archive_log: reading file contents to redo log\n");
    // open recovery file
    fp = fopen(fileName, "rb");

    // cannot open file
    if (fp == NULL){
        perror("fopen");
        AK_EPI;
        exit(EXIT_FAILURE);
    }
    
    // read structure from file
    AK_redo_log log_file;
    log_file.number = 0;
    int i = 0;
    fread(&log_file, sizeof(log_file), 1, fp);
    
    // read command by command
    for(i = 0; i < log_file.number; i++) {
        memcpy(redo_log->command_recovery, log_file.command_recovery, sizeof(log_file.command_recovery));
       //redo_log->command_recovery[i].finished = log_file.command_recovery[i].finished;
        //redo_log->command_recovery[i].operation = log_file.command_recovery[i].operation;
    }
    redo_log->number = log_file.number;
    printf("AK_recover_archive_log: Checking for unfinished archived data commands...\nNumber of archived commands: %d\n", redo_log->number);
    for(i = 0; i < redo_log->number; i++) {
        printf("%s", redo_log->command_recovery[i].table_name);
        if(redo_log->command_recovery[i].finished != 1) 
            AK_recovery_insert_row(redo_log->command_recovery[i].table_name, redo_log->command_recovery[i].arguments);
    }
    
    AK_empty_archive_log();
    
    // close
    fclose(fp);
    AK_EPI;
}

/** 
 * Function is given the table name with desired data that should be
 * inserted inside. By using the table name, function retrieves table 
 * attributes names and their types which uses afterwards for insert_data_test
 * function to insert data to designated table.
 * @author Dražen Bandić, updated by Tomislav Turek
 * @brief Inserts a new row in table with attributes
 * @param table - table name to insert to
 * @param attributes - attribute to insert
 * @return no value
 */
void AK_recovery_insert_row(char* table, char** attributes){
    AK_PRO;
    
    printf("AK_recovery: found unfinished archived data commands for %s, executing...\n", table);
    int i;
    // retrieve table attributes names
    char* table_attr_names = AK_rel_eq_get_atrributes_char(table);
    char** attr_name = AK_recovery_tokenize(table_attr_names, ";", 0);
    // retrieve table attribute types
    char* table_attr_types = AK_get_table_atribute_types(table);
    char** attr_types = AK_recovery_tokenize(table_attr_types, ";", 0);
    
    // convert all attribute types to integers
    int type[MAX_ATTRIBUTES];
    for(i = 0; i < MAX_ATTRIBUTES; i++){
        if(attr_types[i] == NULL){
            break;
        }
        type[i] = atoi(attr_types[i]);
    }

    int n = i;
    // insert data to table
    printf("Executing recovered command: %d, %s, %s %s\n", INSERT,
            table, attributes[1], attributes[2]);
    insert_data_test(table, attr_name, attributes, n, type);
    AK_EPI;
}

/** 
 * @author Dražen Bandić
 * @brief Tokenizes the input with the given delimiter and puts them in an double pointer structure (so we can execute an insert)
 * @param input - input to tokenize
 * @param delimiter - delimiter
 * @param valuesOrNot - 1 if the input are values, 0 otherwise
 * @return new double pointer structure with tokens
 */
char** AK_recovery_tokenize(char* input, char* delimiter, int valuesOrNot){
    AK_PRO;
    char* str = strdup(input);
    int count = 0;
    char** result = AK_malloc(MAX_ATTRIBUTES*sizeof(*result));

    char* tok=strtok(str, delimiter); 

    while(1){
        result[count++] = tok? strdup(tok) : tok;

        if (!tok) break;

        tok = strtok(NULL, delimiter);
    } 
    AK_free(str);

    int i = 0;
    int j = 0;

    char** values = AK_malloc(MAX_ATTRIBUTES*sizeof(*result));
    if(valuesOrNot == 1){
        for(i = count - 2, j = 0; i >= 0 && j < count - 1; i--, j++){
            values[j] = result[i];
        }
        AK_free(result);
        AK_EPI;
        return values;
    } else{
        AK_EPI;
        return result;
    }
}

/**
 * this variable flags if system failed
 */
short grandfailure = 0;

/**
 * Function is called when SIGINT signal is sent to the system.
 * All commands that are written to rec.bin file are recovered to
 * the designated structure and then executed.
 * @author Tomislav Turek
 * @brief Function that recovers and executes failed commands
 * @param sig required integer parameter for SIGINT handler functions
 */
void AK_recover_operation(int sig) {
    AK_PRO;
    // set flag that system failed
    grandfailure = 1;
    // acknowledge that the system has failed
    printf("\nUnexpected system failure, trying to recover...\n");
    // recover from failure
    AK_recover_archive_log("../src/rec/rec.bin");
    AK_EPI;
}

/**
 * Function does nothing while waiting a SIGINT signal (signal represents       // doxygen @ for full description ???
 * system failure). Upon retrieving the signal it calls function
 * AK_recover_operation which starts the recovery by building commands.
 * To comply with the designated structure AK_command_recovery_struct           // {link} to struct ???
 * it writes dummy commands to the file log.log
 * @brief Function for recovery testing.
 * @author Tomislav Turek
 */
void AK_recovery_test() {
    AK_PRO;
    printf("Initiating recovery test\n");
    
    // allocate the needed space
    AK_command_recovery_struct *command = AK_malloc(sizeof(AK_command_recovery_struct));
    int i;
    
    // build first command
    command->operation = INSERT;
    sprintf(command->table_name, "student");
    sprintf(command->arguments[0], "35898");
    sprintf(command->arguments[1], "Matija");
    sprintf(command->arguments[2], "Novak");
    sprintf(command->arguments[3], "2000");
    sprintf(command->arguments[4], "180");
    command->finished = 1;
    
    // save first command to redo_log
    redo_log->command_recovery[0] = *command;
    redo_log->number++;
    
    // build second command
    sprintf(command->arguments[0], "36996");
    sprintf(command->arguments[1], "Pero");
    sprintf(command->arguments[2], "Peric");
    sprintf(command->arguments[3], "2004");
    sprintf(command->arguments[4], "88");
    command->finished = 1;
    
    // save second command to redo_log
    redo_log->command_recovery[1] = *command;
    redo_log->number++;
    
    // write commands to file
    AK_archive_log(-10);
    // print instructions
    printf("Working... use Ctrl+C to destabilize the system\n");
    // register handler function
    // sigset(SIGINT, AK_recover_operation); <-- uncomment for testing
    
    // do nothing
    // while(!grandfailure);                 <-- uncomment for testing
    AK_EPI;
}
