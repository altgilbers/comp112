// Simple red-black tree for memorizing key/value pairs 

#include <stdio.h>
#include <stdlib.h>
#include "sglib.h"
#include "help.h"

#define TRUE 1
#define FALSE 0 

typedef struct rbtree {
  char key[KEYLEN]; 
  char value[VALLEN];
  char color_field;
  struct rbtree *left;
  struct rbtree *right;
} rbtree;

#define CMPARATOR(x,y) (strncmp((x->key),(y->key),(KEYLEN)))

SGLIB_DEFINE_RBTREE_PROTOTYPES(rbtree, left, right, color_field, CMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(rbtree, left, right, color_field, CMPARATOR);

static struct rbtree *the_tree = NULL; 

int rbtree_put(const char *key, const char *value) { 
    struct rbtree target; 
    struct rbtree *found; 
    struct rbtree *node; 
    strncpy(target.key, key, KEYLEN); 
    if ((found=sglib_rbtree_find_member(the_tree, &target))==NULL) {
      node = (struct rbtree *)malloc(sizeof(struct rbtree));
      strncpy(node->key, key, KEYLEN);
      strncpy(node->value, value, VALLEN);
      sglib_rbtree_add(&the_tree, node);
      return TRUE; 
    } else { 
      strncpy(found->value, value, VALLEN); 
      return FALSE; 
    } 
} 

int rbtree_get(const char *key, char *value) { 
    struct rbtree target; 
    struct rbtree *found; 
    strncpy(target.key, key, KEYLEN); 
    if ((found=sglib_rbtree_find_member(the_tree, &target))!=NULL) {
      strncpy(value, found->value, VALLEN);
      return TRUE; 
    } else { 
      return FALSE; 
    } 
} 

void rbtree_print() { 
    struct rbtree *te; 		// tree element 
    struct sglib_rbtree_iterator  it;	// tree iterator 

    for(te=sglib_rbtree_it_init_inorder(&it,the_tree); 
        te!=NULL; te=sglib_rbtree_it_next(&it)) {
        printf("  %s => %s\n", te->key, te->value);
    }
    printf("\n");
} 

