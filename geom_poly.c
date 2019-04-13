#include "db.h"
#include "token.h"
#include "xwin.h"
#include "rubber.h"
#include "rlgetc.h"
#include "opt_parse.h"
#include <string.h>

#define NORM 0		/* drawing modes */
#define RUBBER 1	 

static COORDS *CP;

DB_TAB dbtab; 
DB_DEFLIST dbdeflist;
DB_POLY dbpoly;
void draw_poly(); 

/* FIXME: this code should be a bit more robust.  It should
automatically cull any colinear points as they are entered, should
eliminate any consecutive duplicate points, should not allow any polys
with less than three vertices, etc...  The user should not be able to
enter a malformed geometry. */

/*
 *
 *  "ADD"--+---"P"------+-+-------------+--|xy|-|xy|-+-|xy|-+--<CMD|EOC>-->
 *         |            v |             v            |      v 
 *         +-"P"<layer>-+ +-":W"<width>-+            +------+
 *                        |             |
 *                        +-":F"--------+
 */

OPTS opts;

static double x1, y1;
static double xx, yy;

int add_poly(LEXER *lp, int *layer)
{
    enum {START,NUM1,COM1,NUM2,END,ERR} state = START;

    int debug=0;
    int done=0;
    int nsegs=0;
    TOKEN token;
    char *word;
    static double xold, yold;

    if (debug) printf("in add poly layer %d\n", *layer);

    opt_set_defaults(&opts);

    if (debug) {printf("in geom_poly: layer %d\n",*layer);}
    rl_saveprompt();
    rl_setprompt("ADD_POLY> ");

    while (!done) {
	token = token_look(lp, &word);
	if (debug) printf("got %s: %s\n", tok2str(token), word);
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	    case START:		/* get option or first xy pair */
		db_checkpoint(lp);
		nsegs=0;
		if (debug) printf("in start\n");
		if (token == OPT ) {
		    token_get(lp, &word); 
		    if (opt_parse(word, POLY_OPTS, &opts) == -1) {
		    	state = END;
		    } else {
			state = START;
		    }
		} else if (token == NUMBER) {
		    state = NUM1;
		} else if (token == EOL) {
		    token_get(lp, &word); 	/* just eat it up */
		    state = START;
		} else if (token == EOC || token == CMD) {
		    done++;
		} else {
		    token_err("POLY", lp, "expected OPT or NUMBER", token);
		    state = END; 
		} 
		xold=yold=0.0;
		break;
	    case NUM1:		/* get pair of xy coordinates */
		if (debug) printf("in num1, nsegs=%d\n", nsegs);
		if (token == EOL) {
		    token_get(lp, &word); 	/* just ignore it */
		} else if (token == EOC) {
		    state = END; 
		} else if (token == CMD) {
		    state = END; 
		} else if (getnum(lp, "POLY", &xx, &yy)) {
		    xold=x1; yold=y1;
		    x1 = xx; y1 = yy;
		    nsegs++;

		    if (nsegs == 1) {
			CP = coord_new(x1,y1);
			coord_append(CP, x1,y1);
			if (debug) coord_print(CP);
			rubber_set_callback(draw_poly);
			state = NUM1;
		    } else if (nsegs<=3 && x1==xold && y1==yold) {

			/* two identical clicks terminates this poly */
			/* but keeps the ADD P command in effect */

	    		rubber_clear_callback();
			printf("POLY: not enough distinct segments\n");
			state = START;
		    } else if (x1==xold && y1==yold) {

		    	if (debug) coord_print(CP);
			if (debug) printf("dropping coord\n");
			coord_drop(CP);  /* drop last coord */
		    	if (debug) coord_print(CP);

		    	if (nsegs >= 3) {
			    if (debug) printf("adding poly1 layer %d, nsegs=%d\n",
				*layer, nsegs);
			    db_add_poly(currep, *layer, opt_copy(&opts), CP);
		    	} else {
			    printf("   POLY requires at least three points\n");
			}
			   
			need_redraw++;
			rubber_clear_callback();
			state = START;
		    } else {
			rubber_clear_callback();
			if (debug) printf("doing append\n");
			coord_swap_last(CP, x1, y1);
			coord_append(CP, x1,y1);
			rubber_set_callback(draw_poly);
			rubber_draw(x1,y1, 0);
			state = NUM1;	/* loop till EOC */
		    }
		} else {
		    token_err("POLY", lp, "expected NUMBER", token);
		    state = END; 
		}
		break;
	    case END:
		if (debug) printf("in end\n");
		if (token == EOC) {
		    token_get(lp, &word);	/* eat it */
		    if (debug) printf("stream = %s\n", lp->name);
		    if (strcmp(lp->name , "STDIN") != 0) {
			coord_drop(CP);  	/* drop last coord */
		    }
		    if (debug) printf("x1 %g, y1 %g, xold %g, yold %g\n",
		    	x1, y1, xold, yold);
		    if (x1==xold && y1==yold) {
			if (debug) printf("dropping coord\n");
			coord_drop(CP);  /* drop last coord */
			nsegs--;
		    }
		    if (debug) printf("adding poly2 layer %d\n", *layer);
		    if (nsegs >= 2) {
		    	if (debug) coord_print(CP);
			if (debug) printf("adding poly1 layer %d nsegs=%d\n",
				*layer, nsegs);
			db_add_poly(currep, *layer, opt_copy( & opts), CP);
			need_redraw++;
		    } else {
			printf("   POLY requires at least three points\n");
			rubber_clear_callback();
		    }
		} else if (token == CMD) {
		    printf("   cancelling ADD POLY\n");
		    done++;
		} else {
		    token_flush_EOL(lp);
		}
		done++;
		break;
	    default:
	    	break;
	}
    }
    rl_restoreprompt();
    rubber_clear_callback();
    return(1);
}

/* make  poly_check(CP) to remove collinear points and dups */

void draw_poly(double x2, double y2, int count) 
{
	// static double x1old, x2old, y1old, y2old;
	int debug=0;
	BOUNDS bb;
	static int called = 0;

	bb.init=0;


	/* DB_TAB dbtab;  */
	/* DB_DEFLIST dbdeflist; */
	/* DB_POLY dbpoly; */


        dbtab.dbhead = &dbdeflist;
        dbtab.next = NULL;
	dbtab.name = "callback";

        dbdeflist.u.p = &dbpoly;
        dbdeflist.type = POLY;

        dbpoly.layer=1;
        dbpoly.opts=&opts;
        dbpoly.coords=CP;
	
	if (debug) {printf("in draw_poly\n");}

	if (count == 0) {		/* first call */
	    jump(&bb, D_RUBBER); /* draw new shape */
	    do_poly(&dbdeflist, &bb, D_RUBBER);
	    called++;
	} else if (count > 0) {		/* intermediate calls */
	    jump(&bb, D_RUBBER); /* erase old shape */
	    do_poly(&dbdeflist, &bb, D_RUBBER);
	    jump(&bb, D_RUBBER); /* draw new shape */
	    coord_swap_last(CP, x2, y2);
	    do_poly(&dbdeflist, &bb, D_RUBBER);
	    called++;
	} else {			/* last call, cleanup */
	    if (called) {
		jump(&bb, D_RUBBER); /* erase old shape */
		do_poly(&dbdeflist, &bb, D_RUBBER);
	    }
	    called=0;
	}

	/* save old values */
	// x1old=x1;
	// y1old=y1;
	// x2old=x2;
	// y2old=y2;
	jump(&bb, D_RUBBER);
}

