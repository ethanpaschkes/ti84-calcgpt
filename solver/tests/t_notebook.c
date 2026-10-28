/* Run what you would type through the same steps the screen uses and print the lines it would show. */
#include <stdio.h>
#include <string.h>
#include "edit.h"
#include "expr.h"
#include "mathx.h"
#include "notebook.h"
#include "solve.h"

static int fails = 0;

static void run(const char *typed, const char *must)
{
    struct hentry h;
    char lhs[ED_MAX + 1], rhs[ED_MAX + 1], all[3000] = "";
    int i;
    static struct mx_solution sol;
    struct mx_eval ev;

    memset(&h, 0, sizeof h);
    strcpy(h.input, typed);
    ed_split(typed, lhs, rhs, ED_MAX + 1);
    if (rhs[0]) { mx_solve(lhs, rhs, &sol); nb_build_solution(&h, lhs, rhs, &sol); }
    else { mx_evaluate(lhs, &ev); nb_build_eval(&h, &ev); }
    printf("> %s\n", typed);
    for (i = 0; i < h.nlines; i++) {
        printf("    %s%s\n", h.is_step[i] ? "  " : "", h.text[i]);
        strcat(all, h.text[i]);
        strcat(all, "\n");
    }
    if (must && !strstr(all, must)) { printf("    ^^^ FAIL expected '%s'\n", must); fails++; }
    puts("");
}

int main(void)
{
    mx_degrees = true;
    puts("===== EQUATIONS");
    run("x^2=4x-3", "x = 1  or  x = 3");
    run("2x+3=11", "x = 4");
    run("[x+1|2]=3", "x = 5");
    run("x^2+x=1", "x = [-1 - sqrt(5)|2]");
    run("x^2=-1", "No solution");
    run("x^3-6x^2+11x-6=0", "x = 1  or  x = 2  or  x = 3");
    run("2x^2+x=1", "x = -1  or  x = [1|2]");
    puts("===== TRIG EQUATIONS (degrees)");
    run("sin(x)=[1|2]", "x = 30  or  x = 150");
    run("2cos(x)=sqrt(3)", "x = 30  or  x = 330");
    run("sin(2x)=[1|2]", "x = 15");
    run("tan(x)=1", "x = 45  or  x = 225");
    run("cos(x)=-1", "x = 180");
    mx_degrees = false;
    puts("===== TRIG EQUATIONS (radians)");
    run("sin(x)=[sqrt(3)|2]", "x = [pi|3]  or  x = [2pi|3]");
    run("2sin(x)=1", "[5pi|6]");
    mx_degrees = true;
    puts("===== YOUR 8 PROOFS (typed with stacked fractions)");
    run("sin(x)^4-cos(x)^4=sin(x)^2-cos(x)^2", "Proof 1");
    run("1-[cos(x)^2|1+sin(x)]=sin(x)", "Proof 2");
    run("[sin(x)|1+cos(x)]=csc(x)-cot(x)", "Proof 3");
    run("[1+tan(x)|sin(x)]=csc(x)+sec(x)", "Proof 4");
    run("[(sec(x)-tan(x))^2+1|csc(x)(sec(x)-tan(x))]=2tan(x)", "Proof 5");
    run("[1-cos(x)|1+cos(x)]=(csc(x)-cot(x))^2", "Proof 6");
    run("tan(x)^2-sin(x)^2=sin(x)^2tan(x)^2", "Proof 7");
    run("[sin(x)^3+cos(x)^3|sin(x)+cos(x)]=1-sin(x)cos(x)", "Proof 8");
    run("sin(x)^2+cos(x)^2=1", "True for every x");
    run("sin(x)=cos(x)", "x = 45");
    puts("===== NUMBERS AND FRACTIONS");
    run("[3|4]+[5|6]=", "= [19|12]");
    run("sqrt(8)=", "2sqrt(2)");
    run("sin(60)=", "[sqrt(3)|2]");
    run("[x^2-1|x-1]=", "= x + 1");
    run("(x+1)(x-3)=", "= x^2 - 2x - 3");
    run("[x^2+3x+2|x^2-1]=", "[x + 2|x - 1]");
    run("1+=", NULL);
    printf(fails ? "%d FAILED\n" : "notebook: all passed\n", fails);
    return fails != 0;
}
