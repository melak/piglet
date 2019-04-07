#include <stdio.h>
#include <string.h>		/* for strchr() */
#include <ctype.h>		/* for toupper() */

#include "rlgetc.h"
#include "db.h"
#include "token.h"
#include "xwin.h" 	
#include "rubber.h"

#define UNUSED(x) (void)(x)

static double x1, y1, x2, y2, x3, y3;
void wrap_draw_box();
STACK *stack;
char buf[MAXFILENAME];
static int instcounter=0;	/* used for generating uniq instance names */

/* 
    wrap a set of components in the current device.
    WRAP [<restrictor>] [<devicename>] { xyorig coord1 coord2 } ... <EOC>
*/

int com_wrap(LEXER *lp, char *arg)		
{
    UNUSED(arg);
    enum {START,NUM1,NUM2,NUM3,END} state = START;

    int done=0;
    TOKEN token;
    char *word;
    int debug=0;
    int valid_comp=0;
    int i;
    DB_DEFLIST *p_best;
    DB_DEFLIST *p_new;
    DB_TAB *newrep;
    OPTS *opts;
    char *wrap_inst_name = NULL;
    double tmp;
    int anonymous=0;	/* 0=user supplied name, 1=autogenerated devicename */

    int my_layer=0; 	/* internal working layer */

    int comp=ALL;

    /* check that we are editing a rep */
    if (currep == NULL ) {
	printf("must do \"EDIT <name>\" before WRAP\n");
	token_flush_EOL(lp);
	return(1);
    }

/* 
    To create a show set for restricting the pick, we look 
    here for and optional primitive indicator
    concatenated with an optional layer number:

	A[layer_num] ;(arc)
	C[layer_num] ;(circle)
	L[layer_num] ;(line)
	N[layer_num] ;(note)
	O[layer_num] ;(oval)
	P[layer_num] ;(poly)
	R[layer_num] ;(rectangle)
	T[layer_num] ;(text)

    or a instance name.  Instance names can be quoted, which
    allows the user to have instances which overlap the primitive
    namespace.  For example:  N7 is a note on layer seven, but
    "N7" is an instance call.

*/

    while(!done) {
	token = token_look(lp,&word);
	if (debug) printf("got %s: %s\n", tok2str(token), word);
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	case START:		/* get option or first xy pair */
	    db_checkpoint(lp);
	    if (debug) printf("in START\n");
	    if (token == OPT ) {
		token_get(lp,&word); 
                if (word[0]==':') {
                    switch (toupper((unsigned char)word[1])) {
                        default:
                            printf("WRAP: bad option: %s\n", word);
                            state=END;
                            break;
                    }
                } else {
                    printf("WRAP: bad option: %s\n", word);
                    state = END;
                }
                state = START;
	    } else if (token == NUMBER) {
		state = NUM1;
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just eat it up */
		state = START;
	    } else if (token == EOC || token == CMD) {
		state = END;
	    } else if (token == IDENT) {
		token_get(lp,&word);
	    	state = START;
		/* check to see if is a valid comp descriptor */
		valid_comp=0;
		if ((comp = is_comp(toupper((unsigned char)word[0])))) {
		    if (strlen(word) == 1) {
			my_layer = default_layer();
			printf("using default layer=%d\n",my_layer);
			valid_comp++;	/* no layer given */
		    } else {
			valid_comp++;
			/* check for any non-digit characters */
			/* to allow instance names like "L1234b" */
			for (i=0; i<strlen(&word[1]); i++) {
			    if (!isdigit((unsigned char)word[1+i])) {
				valid_comp=0;
			    }
			}
			if (valid_comp) {
			    if(sscanf(&word[1], "%d", &my_layer) == 1) {
				if (debug) printf("given layer=%d\n",my_layer);
			    } else {
				valid_comp=0;
			    }
			} 
			if (valid_comp) {
			    if (my_layer > MAX_LAYER) {
				printf("layer must be less than %d\n",
				    MAX_LAYER);
				valid_comp=0;
				done++;
			    }
			    if (!show_check_modifiable(currep, comp, my_layer)) {
				printf("layer %d is not modifiable!\n",
				    my_layer);
				token_flush_EOL(lp);
				valid_comp=0;
				done++;
			    }
			}
		    }
		} 

		if (valid_comp==0) {
		    /* must be the new name of the wrapped instance */ 
		    if (db_exists(word)) {
		        printf("can't wrap onto an existing cell!\n");
		        state = END;
		    } else {
			wrap_inst_name = strsave(word);
			if (debug) printf("got %s wrap_inst_name\n", wrap_inst_name);
		    }
		}
	    } else {
		token_err("WRAP", lp, "expected DESC or NUMBER", token);
		state = END;	/* error */
	    }
	    break;
	case NUM1:		/* get pair of xy coordinates */
	    if (debug) printf("in NUM1\n");
	    if (token == NUMBER) {
		if (getnum(lp, "WRAP", &x1, &y1)) {
		    state = NUM2; 
		} else {
		    state = END;
		}
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("WRAP: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("WRAP", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;

        case NUM2:              /* get pair of xy coordinates */
	    if (debug) printf("in NUM2\n");
	    if (token == NUMBER) {
		if (getnum(lp, "WRAP", &x2, &y2)) {
		    rubber_set_callback(wrap_draw_box);
		    state = NUM3;
	    	} else {
		    state = END;
		}
	    } else if (token == EOL) {
                token_get(lp,&word);     /* just ignore it */
            } else if (token == EOC || token == CMD) {
                printf("IDENT: cancelling POINT\n");
                state = END;
            } else {
                token_err("IDENT", lp, "expected NUMBER", token);
                state = END;
            }
            break;

	case NUM3:
	    if (debug) printf("in NUM6\n");
	    if (token == NUMBER) {
		if (getnum(lp, "WRAP", &x3, &y3)) {
		    printf("got %g %g\n", x3, y3);
		    rubber_clear_callback();
		    
		    if (x2 > x3) {
		       tmp = x3; x3 = x2; x2 = tmp;
		    }
		    if (y2 > y3) {
		       tmp = y3; y3 = y2; y2 = tmp;
		    }

		    stack=db_ident_region(currep, x2, y2, x3, y3, 1, my_layer, comp, 0);
		    
		    if (stack==NULL) {
			printf("Nothing here to wrap.  Try \"SHO #E\"?\n");
		    } else {
			if (wrap_inst_name == NULL) {
			    anonymous++;	/* autogenerated name */
			    sprintf(buf, "NONAME_%03d", instcounter++);
			    if (debug) printf("autonaming rep %s\n", buf);
			    wrap_inst_name = buf;
			}
			newrep = (DB_TAB *) db_install(wrap_inst_name);
			if (debug) printf("doing db_install for rep %s, ptr = %ld\n",
			    buf, (long int) newrep);

			// for each selected item in the stack,
			// copy the item and put it in wrap_inst_name
			// take it out of the original
			// fix up the coordinates to account for being wrapped
			// put it in the new wrap instance

			while ((p_best = (DB_DEFLIST *) stack_pop(&stack))!=NULL) {
			    p_new = db_copy_component(p_best, NULL);
			    if (debug) printf("copying %ld, p_new = %ld\n", 
				(long int) p_best, (long int) p_new);
			    db_unlink_component(currep, p_best);
			    db_move_component(p_new, -x1, -y1);
			    db_insert_component(newrep, p_new);
			}

			// db_fsck(currep->dbhead);
			// add the new wrap instance to the currep

			opts = opt_create();
			opt_set_defaults(opts);
			show_init(newrep);
			newrep->minx = min(x2-x1, x3-x1);
			newrep->maxx = max(x2-x1, x3-x1);
			newrep->miny = min(y2-y1, y3-y1);
			newrep->maxy = max(y2-y1, y3-y1);
			db_add_inst(currep, newrep, opts, x1, y1);

			newrep->modified++;

			if (anonymous) {
			    newrep->is_tmp_rep++;	// flag as a tmp rep
			} else {
			    db_save(lp, newrep, wrap_inst_name);
			    newrep->modified=0;
			} 
			if (!anonymous) {
			   free(wrap_inst_name);	// just free the name string
			}
			wrap_inst_name=NULL;
			currep->modified++;
			need_redraw++;
		    }
		    state = END; 			// BUG? FIXME
		} else {
		    state = END;
		}
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("WRAP: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("WRAP", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case END:
	default:
	    if (token == EOC || token == CMD) {
		;
	    } else {
		token_flush_EOL(lp);
	    }
	    done++;
	    rubber_clear_callback();
	    break;
	}
    }
    return(1);
}


void wrap_draw_box(double x3, double y3, int count)
{
        static double x2old, x3old, y2old, y3old;
        BOUNDS bb;
        bb.init=0;

        if (count == 0) {               /* first call */
            db_drawbounds(x2,y2,x3,y3,D_RUBBER);                /* draw new shape */
        } else if (count > 0) {         /* intermediate calls */
            db_drawbounds(x2old,y2old,x3old,y3old,D_RUBBER);    /* erase old shape */
            db_drawbounds(x2,y2,x3,y3,D_RUBBER);                /* draw new shape */
        } else {                        /* last call, cleanup */
            db_drawbounds(x2old,y2old,x3old,y3old,D_RUBBER);    /* erase old shape */
        }

        /* save old values */
        x2old=x2;	/* global */
        y2old=y2;
        x3old=x3;
        y3old=y3;
        jump(&bb, D_RUBBER);
}


