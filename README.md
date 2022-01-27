# Files-Similarity

Isaac Chun

Benjamin Lee

This program analyzes set of files then calculates Jensen_Shannon distance between the word frequencies of pairs of files.

We checked for error cases:

    1. When the number of directory threads, file threads or analysis threads given by user is < 0,
        print error and exit
    2. When there is optional argument, but it is missing input (except for suffix) EX: -d, -f
        print error and exit
        - If -s is given the program checks for all files.
    3. If total number of files to be compared is < 2
        print error and exit
    
We also tested many edge cases and different inputs:

    We made test cases and manually checked for the correct JSD values, then ran our program which gave the same answer.

    We tested for multiple cases for different kinds of files:
        1. Where we only had file arguments
        2. Where we only had directory arguments
        3. Where we had file and directory arguments
        4. Where we had nested directories and files throughout all of the directories
        5. Nested directories and files throughout the directories and file arguments
        6. When the file does not end with given suffix
            - ignore the file and will not enqueue

    We checked to make sure the JSD was correct with different text files:
        1. Where all the text files were empty
        2. Where they contained some of the same words
        3. Where they contained all of the same words
            JSD equals 0
        4. Where they contained no similar words
            JSD equals 1

    We also checked to make sure this worked with different amounts of threads:
        1. 1 thread for each (default)
        2. 1 thread for all but one, and tried different amount of threads for the one we changed
            Ex. 2 file threads, 1 of all other threads
        3. Multiple threads for each

    We also checked for directory entries that started with . and made sure to exclude them from being enqueued.



