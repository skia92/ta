# WC
## Name
word, line, character, and byte count

## Synopsis
wc (-clmw) (file ...)

## Description
The wc utility displays the number of lines, words, and bytes contained in each input **file**, or standard input (if
no file is specified) to the standard output. Characters beyond the final *newline* character will not be included in the line count.

A word is defined as a string of characters delimited by white space characters. White space characters are the set of
characters for which the iswspace(3) function returns true. If more than one input file is specified, a line of
cumulative conunts for all the files is displayed on a seperate line after the output for the last file. 
