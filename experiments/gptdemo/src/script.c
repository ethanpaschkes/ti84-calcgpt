#include "script.h"
#include <string.h>

const struct photo_script PHOTOS[N_PHOTOS] = {
    {"2x+3=11", "2x + 3 = 11",
     "I read: 2x + 3 = 11\n\nStep 1: subtract 3 from both sides\n2x = 8\n\nStep 2: divide both sides by 2\nx = 4\n\nCheck: 2(4) + 3 = 11. Correct.",
     "Today 9:41"},
    {"x^2-5x+6=0", "x^2 - 5x + 6 = 0",
     "I read: x^2 - 5x + 6 = 0\n\nFactor: find two numbers that multiply to 6 and add to -5. They are -2 and -3.\n(x - 2)(x - 3) = 0\n\nSo x = 2 or x = 3\n\nCheck: 2^2 - 10 + 6 = 0 and 3^2 - 15 + 6 = 0. Both work.",
     "Today 8:15"},
    {"2sin x=1", "2 sin x = 1",
     "I read: 2 sin x = 1\n\nI will find the angles from 0 to 360 degrees.\n\nDivide by 2: sin x = 1/2\n\nsin is 1/2 at the reference angle 30 degrees, and sin is positive in quadrants I and II.\n\nx = 30 deg or x = 150 deg\n\nGeneral form: x = 30 + 360n or x = 150 + 360n",
     "Yesterday"},
};

/* does `text` (any case) contain `word`? */
static int has(const char *text, const char *word)
{
    size_t n = strlen(word), i, j;
    for (i = 0; text[i]; i++) {
        for (j = 0; j < n && text[i + j]; j++) {
            char a = text[i + j], b = word[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (a != b) break;
        }
        if (j == n) return 1;
    }
    return 0;
}

const char *script_reply(const char *q)
{
    if (has(q, "who are you") || has(q, "what are you") || has(q, "your name"))
        return "I'm CalcGPT, a small language model running entirely on this calculator's processor. No internet and no cloud. Ask me to solve an equation, or tap Y= to import a photo of one.";
    if (has(q, "thank")) return "You're welcome! Send me another problem whenever you like.";
    if (has(q, "law of sines") || has(q, "sine rule"))
        return "The Law of Sines: a / sin A = b / sin B = c / sin C.\n\nUse it when you know two angles and a side, or two sides and an angle that is not between them (watch for two possible triangles).";
    if (has(q, "law of cosines") || has(q, "cosine rule"))
        return "The Law of Cosines: c^2 = a^2 + b^2 - 2ab cos C.\n\nUse it for two sides and the angle between them, or for all three sides to find an angle.";
    if (has(q, "quadratic"))
        return "For ax^2 + bx + c = 0:\n\nx = (-b +/- sqrt(b^2 - 4ac)) / (2a)\n\nThe discriminant b^2 - 4ac is positive for two real roots, zero for one, negative for none.";
    if (has(q, "pythag"))
        return "In a right triangle, a^2 + b^2 = c^2, where c is the hypotenuse. For example 3, 4, 5: 9 + 16 = 25.";
    if (has(q, "unit circle"))
        return "On the unit circle the point at angle t is (cos t, sin t). Key angles: 30 deg = (sqrt(3)/2, 1/2), 45 deg = (sqrt(2)/2, sqrt(2)/2), 60 deg = (1/2, sqrt(3)/2).";
    if (has(q, "identity") || has(q, "identities"))
        return "The big three: sin^2 x + cos^2 x = 1, 1 + tan^2 x = sec^2 x, and 1 + cot^2 x = csc^2 x. Most proofs are about rewriting one side until it matches the other.";
    if (has(q, "sin") && has(q, "30"))
        return "sin(30 deg) = 1/2.\n\nIn a 30-60-90 triangle the side opposite the 30 degree angle is half the hypotenuse, and sine is opposite over hypotenuse.";
    if (has(q, "cos") && has(q, "60"))
        return "cos(60 deg) = 1/2.\n\nIn a 30-60-90 triangle the side next to the 60 degree angle is half the hypotenuse.";
    if (has(q, "joke"))
        return "Why was the equal sign so humble? It knew it wasn't less than or greater than anyone else.";
    if (has(q, "hello") || has(q, "hi ") || has(q, "hey") || strlen(q) <= 2)
        return "Hi! I'm CalcGPT, running right here on your calculator. What are we solving?";
    return "I'm running fully on-device, so I stick to math and science. Try: law of sines, quadratic formula, sin 30, or tap Y= to import a photo of a problem.";
}

int wrap_text(const char *text, int width, int *starts, int *lens, int max)
{
    int n = 0, i = 0, len = (int)strlen(text);
    while (n < max) {
        int line_start = i, last_space = -1, j = i;
        if (i > len) break;
        while (j < len && text[j] != '\n' && j - line_start < width) {
            if (text[j] == ' ') last_space = j;
            j++;
        }
        if (j < len && text[j] != '\n' && text[j] != ' ' && j - line_start == width && last_space >= line_start) j = last_space;  /* the next word does not fit: break at the last space */
        starts[n] = line_start;
        lens[n] = j - line_start;
        n++;
        if (j >= len) break;
        i = (text[j] == '\n' || text[j] == ' ') ? j + 1 : j;
        if (i == len && text[j] != '\n') break;
    }
    return n;
}
