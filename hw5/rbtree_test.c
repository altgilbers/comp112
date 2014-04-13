#include <stdio.h> 
#include "rbtree.h" 
#include "help.h" 

void test_rbtree_get(const char key[KEYLEN], char value[VALLEN]) { 
    int result = rbtree_get(key,value); 
    printf("calling get(%s,?) ... result is %d\n", key, result); 
    if (result) { 
	printf("    get returns %s=>%s\n", key, value); 
    } else { 
	printf("    get returns %s=>NONE\n", key); 
    } 
} 

void test_rbtree_put(const char key[KEYLEN], const char value[VALLEN]) { 
    int result = rbtree_put(key,value); 
    printf("calling put(%s,%s) ... result is %d\n", key, value, result); 
    if (result) { 
	printf("    put creates new key %s=>%s\n", key, value); 
    } else { 
	printf("    put updates existing key %s=>%s\n", key, value);
    } 

} 

main()
{ 
        char value[VALLEN]; 
	test_rbtree_put("foo", "bar"); 
	test_rbtree_put("foo", "cat"); 
	test_rbtree_put("foo", "cat"); 
	test_rbtree_put("bar", "none"); 
	test_rbtree_put("fi", "dough"); 
	test_rbtree_put("int", "eger"); 
	test_rbtree_put("foo", "cat"); 
	test_rbtree_get("foo", value); 
	test_rbtree_get("tho", value); 
	printf("Contents of tree:\n"); 
        rbtree_print(); 
} 
