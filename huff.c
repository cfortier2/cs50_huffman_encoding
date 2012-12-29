/*
 *Huff.c by Chris Fortier
 */
 
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "huffile.h"
#include "forest.h"
#include "tree.h"


//prototypes
Plot* contains (Forest *f, char c);
bool build_tree(Forest *f);
int encode(Tree *t, char c, int hops, char c_array[], char *target);

//globals
int checksum_counter = 0;
int frequencies[SYMBOLS];

//array of encoded bits
char encoded[SYMBOLS][256];
    
//main
int main (int argc, char* argv[])
{
    //keep frequencies in array
    for (int i = 0; i < SYMBOLS; i++)
        frequencies[i] = 0;
    
    //make sure user inputs two arguments
    if (argc != 3)
    {
        printf("You must enter %s input output\n", argv[0]);
        return 1;
    }
    
    //open input 
    FILE *fp = fopen(argv[1],"r" );
    if (fp == NULL)
    {
        printf("Could not open souce file: %s\n", argv[1]);
        return 1;
    }
    
    //open output
    Huffile *outfile = hfopen(argv[2], "w");
    if (outfile == NULL) 
    {
        printf("Could not open destination file: %s\n", argv[2]);
        return 1;
    }   
    
    //create a forest
    Forest *myForest = mkforest();
    if (myForest == NULL)
    {
    	printf("The forest could not be created :(\n");
    	return 1;
    }
    
    //read each character from source file
    for (int c = fgetc(fp); c!= EOF; c = fgetc(fp))
    {
        //printf("%c", c);
        
        //increment frequencies for char
        frequencies[c]++;
        
        //increment checksum
        checksum_counter++;
    }   
    
    //make and plant trees in sorted order
    for (int i = 0; i < SYMBOLS; i++)
    {
        if (frequencies[i] != 0)
        {
            //make the tree
            Tree *tempTree = mktree();
            tempTree->symbol = i;
            tempTree->frequency = frequencies[i];
            tempTree->left = NULL;
            tempTree->right = NULL;
            
            //plant tree in forest
            if (plant(myForest, tempTree) == false)
                printf("Could not plant tree %c\n", i);
        }
    }
    
    //build the huffman tree
    while (myForest->first->tree->frequency < checksum_counter)
    {
        //make the tree
        Tree *tempTree = mktree();
        tempTree->symbol = 0x00;
        tempTree->left = pick(myForest);
        tempTree->right = pick(myForest);
        if (tempTree->right != NULL)
            tempTree->frequency = tempTree->left->frequency + tempTree->right->frequency;
        else
            tempTree->frequency = tempTree->left->frequency;
            
        //plant tree in forest
        if (plant(myForest, tempTree) == false)
            printf("Could not plant parent tree\n");
    }
    
    //create encoding of each character
    for (int i = 0; i < SYMBOLS; i++)
    {
        if (frequencies[i] != 0)
        {
            //create a temporary array to store the encoded value
            char temp[256];
            for (int j = 0; j < 256; j++)
                temp[j] = '9';
            
            encode(myForest->first->tree, i, 0, "", &temp[0]);
            
            //copy temp to encoded
            strncpy(encoded[i], temp, strlen(temp));
            
            //printf("temp: %s\n", temp);
            //printf("encoded: %c, hops: %s ", i, encoded[i]);
        }
    }
    
    //create huffeader header
    Huffeader *header = malloc(sizeof(Huffeader));
    header->magic = MAGIC;
    for (int i = 0; i < SYMBOLS; i++)
        header->frequencies[i] = frequencies[i];
    header->checksum = checksum_counter;
    
    //write header
    hwrite(header, outfile);
   
    //move to beginning of source file
    rewind(fp);
    
    //keep track of how many bits we use
    int bit_count = 0;
    
    //read file per character, lookup frequency, and write to outfile
    for (int c = fgetc(fp); c!= EOF; c = fgetc(fp))
    {
        for (int i = 0; i < strlen(encoded[c]); i++)
        {
            bit_count++;
            
            int bit = 0;
            
            //write each bit
            if (encoded[c][i] == '0')
                bit = 0;
            else if (encoded[c][i] == '1') 
                bit = 1;
                
            bool write_output = bwrite(bit, outfile);
            if (write_output == false)
                printf("Error bwriting: %c\n", encoded[c][i]);
                
            //printf("%c bit_count: %d\n", encoded[c][i], bit_count);
        }
         
        //printf("%s", encoded[c]);
    }
    
    //figure out how many bits in second to last byte are used
    int bits_used = 0;
    if (bit_count <= 8)
        bits_used = bit_count;
    else
        bits_used = (bit_count % 8);
    
    //write trailing bits
    for (int i = 8 - bits_used; i > 1; i--)
    {
        //printf("0\n");
        
        bool write_output = bwrite(0, outfile);
        if (write_output == false)
                printf("Error bwriting: %c\n", '0');
    }  
    
    //cleanup
    outfile->ith = bits_used;
    free(header);
    rmforest(myForest);
    hfclose(outfile);
    fclose(fp);
    
    return 0;
}

//takes a plot and a char for input. Searches the plots for that char. Returns a pointer to the plot if found, otherwise returns NULL.
Plot* contains (Forest *f, char c)
{
    //point cursor to first plot
    Plot *p_cursor = f->first;
    
    //Return NULL if plot is empty
    if (f->first == NULL)
       return NULL;
    
    //check if that plot's tree contains the char c
    while(p_cursor->next != NULL)
    {
        if (p_cursor->tree->symbol == c)
            return p_cursor;
        else
        {
            //move to next plot
            p_cursor = p_cursor->next;
        }
    }
    
    //the character was not found anywhere in the forest
    return NULL;
}

// search the binary tree and create an array of encoded bits
int encode(Tree *t, char c, int hops, char c_array[], char *target)
{   
    //found the char
    if (t->symbol == c)
    {
        strncpy(target, c_array, strlen(c_array));
        target[strlen(c_array)] = '\0';
        //printf("char: %c, hops: %d, c_array[]: %s \n", c, hops, c_array);
        return hops;
    }
    //still looking for the char
    else
    {
        int left = 0;
        int right = 0;
        
        int new_length = strlen(c_array) + 2;
        char temp[new_length];
        strcpy(temp, c_array);
        
        //check the left node
        if (t->left != NULL)
        {
            temp[new_length - 2] = '0';
            temp[new_length - 1] = '\0';
            
            //recursively check the left node
            left = encode(t->left, c, hops + 1, temp, target );
        }
        
        //check the right node
        if (t->right != NULL)   
        {
            temp[new_length - 2] = '1';
            temp[new_length - 1] = '\0';
            
            //recursively check the right node
            right = encode(t->right, c, hops + 1, temp, target);
        }
          
    }
        // somehow the char wasn't found in the tree. Really this is here to make clang happy :)
        return 1;
}


























