/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2019, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#include <stdlib.h>
#include <string.h>

#include <types.h>
#include <class.h>
#include <stack.h>
#include <mm.h>
#include <thread.h>
#include <exceptions.h>
#include <bc_interp.h>
#include <gc.h>

extern jthread_t * cur_thread;

/* 
 * Maps internal exception identifiers to fully
 * qualified class paths for the exception classes.
 * Note that the ones without fully qualified paths
 * will not be properly raised. 
 *
 * TODO: add the classes for these
 *
 */
static const char * excp_strs[16] __attribute__((used)) =
{
	"java/lang/NullPointerException",
	"java/lang/IndexOutOfBoundsException",
	"java/lang/ArrayIndexOutOfBoundsException",
	"IncompatibleClassChangeError",
	"java/lang/NegativeArraySizeException",
	"java/lang/OutOfMemoryError",
	"java/lang/ClassNotFoundException",
	"java/lang/ArithmeticException",
	"java/lang/NoSuchFieldError",
	"java/lang/NoSuchMethodError",
	"java/lang/RuntimeException",
	"java/io/IOException",
	"FileNotFoundException",
	"java/lang/InterruptedException",
	"java/lang/NumberFormatException",
	"java/lang/StringIndexOutOfBoundsException",
};

int 
hb_excp_str_to_type (char * str)
{
    for (int i = 0; i < sizeof(excp_strs)/sizeof(char*); i++) {
        if (strstr(excp_strs[i], str))
                return i;
    }
    return -1;
}



/*
 * Throws an exception given an internal ID
 * that refers to an exception type. This is to 
 * be used by the runtime (there is no existing
 * exception object, so we have to create a new one
 * and init it).
 *
 * @return: none. exits on failure.
 *
 */
// WRITE ME
void
hb_throw_and_create_excp (u1 type)
{
	java_class_t * excp_cls = hb_get_or_load_class(excp_strs[type]);
	obj_ref_t * excp_inst = gc_obj_alloc(excp_cls);
	hb_push_ctor_frame(cur_thread, excp_inst);
	hb_throw_exception(excp_inst);
	return;
}



/* 
 * gets the exception message from the object 
 * ref referring to the exception object.
 *
 * NOTE: caller must free the string
 *
 */
static char *
get_excp_str (obj_ref_t * eref)
{
	char * ret;
	native_obj_t * obj = (native_obj_t*)eref->heap_ptr;
		
	obj_ref_t * str_ref = obj->fields[0].obj;
	native_obj_t * str_obj;
	obj_ref_t * arr_ref;
	native_obj_t * arr_obj;
	int i;
	
	if (!str_ref) {
		return NULL;
	}

	str_obj = (native_obj_t*)str_ref->heap_ptr;
	
	arr_ref = str_obj->fields[0].obj;

	if (!arr_ref) {
		return NULL;
	}

	arr_obj = (native_obj_t*)arr_ref->heap_ptr;

	ret = malloc(arr_obj->flags.array.length+1);

	for (i = 0; i < arr_obj->flags.array.length; i++) {
		ret[i] = arr_obj->fields[i].char_val;
	}

	ret[i] = 0;

	return ret;
}

static inline void 
push_val (var_t v)
{
	op_stack_t * stack = cur_thread->cur_frame->op_stack;

    if (stack->max_oprs < stack->sp + 1) 
        HB_WARN("Stack overflow!");
        
	stack->oprs[++(stack->sp)] = v;
}

/*
 * Throws an exception using an
 * object reference to some exception object (which
 * implements Throwable). To be used with athrow.
 * If we're given a bad reference, we throw a 
 * NullPointerException.
 *
 * @return: none. exits on failure.  
 *
 */
void
hb_throw_exception (obj_ref_t * eref)
{	
	if(!eref) hb_throw_and_create_excp(EXCP_NULL_PTR);
	
	int i;
	native_obj_t * excp_native_obj = (native_obj_t *) eref->heap_ptr;
	const char * excp_class_name = hb_get_class_name(excp_native_obj->class);
	// if(excp_class_name) printf("%s\n", excp_class_name); else printf("null");

	excp_table_t * excp_table = cur_thread->cur_frame->minfo->code_attr->excp_table;
	CONSTANT_Class_info_t * excp_candidate;
	const char * excp_cand_type_name;

	for(i=0; i<cur_thread->cur_frame->minfo->code_attr->excp_table_len; i++){
		excp_candidate = (CONSTANT_Class_info_t * )cur_thread->cur_frame->cls->const_pool[excp_table[i].catch_type];
		excp_cand_type_name = hb_get_const_str(excp_candidate->name_idx, cur_thread->class);
		// printf("%s\n", excp_cand_type_name);
		// printf("%u\n", cur_thread->cur_frame->pc);
		// printf("%u\n", excp_table[i].start_pc);
		// printf("%u\n", excp_table[i].end_pc);
		
		if (!strcmp(excp_class_name, excp_cand_type_name))
			if(cur_thread->cur_frame->pc >= excp_table[i].start_pc && cur_thread->cur_frame->pc <= excp_table[i].end_pc){
				var_t v;
				v.obj = eref;
				push_val(v);
				cur_thread->cur_frame->pc = excp_table[i].handler_pc;
				return;
			}
	}

	hb_pop_frame(cur_thread);
	if(!cur_thread->cur_frame){
		char * excp_str = get_excp_str(eref);
		if(excp_str) printf("Exception in thread \"main\" %s: %s \n", excp_class_name, excp_str);
		else printf("Exception in thread \"main\" %s \n", excp_class_name);
		free(excp_str);
	    exit(EXIT_FAILURE);
	}
	return hb_throw_exception(eref);
}
