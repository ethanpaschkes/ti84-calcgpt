/* Eliza-style conversation engine: keyword rules with wildcard patterns,
 * pronoun reflection, rotating replies and a one-slot memory. No libraries
 * beyond string.h, so it also runs on a computer for testing. */
#include "eliza.h"
#include "calc.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_WORDS 24
#define WORD_LEN 16
#define MAX_RESP 8
#define MAX_PAT 12
#define OUT_LEN 160

static char words[MAX_WORDS][WORD_LEN];
static int nw;
static int cap_start[MAX_PAT], cap_end[MAX_PAT];
static char out[OUT_LEN];
static char memory[48];
static bool have_mem;
static unsigned none_next, mem_next;

static char *wp, *wend;

static void put_str(const char *s)
{
    while (*s && wp < wend) {
        *wp++ = *s++;
    }
}

struct rule {
    const char *key;
    uint8_t rank;
    bool mem;
    const char *pat;
    uint8_t n;
    uint8_t next;
    const char *resp[MAX_RESP];
};

#define COUNT(...) (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *))
#define RULE(key, rank, mem, pat, ...) { key, rank, mem, pat, COUNT(__VA_ARGS__), 0, { __VA_ARGS__ } }

/* Same-rank rules are tried in order, so specific patterns come first. */
static struct rule rules[] = {
    RULE("bye", 60, false, "*",
         "Goodbye. It was nice talking to you.", "Take care. Come back any time."),
    RULE("computer", 50, false, "*",
         "Do computers worry you?", "Why do you mention computers?",
         "What do you think machines have to do with your problem?",
         "Don't you think computers can help people?", "What about machines worries you?"),
    RULE("name", 15, false, "*",
         "I am not interested in names.", "I've told you before, I don't care about names. Please continue."),
    RULE("remember", 5, false, "* i remember *",
         "Do you often think of (2)?", "Does thinking of (2) bring anything else to mind?",
         "What else do you recall?", "Why do you remember (2) just now?",
         "What in the present situation reminds you of (2)?"),
    RULE("remember", 5, false, "* do you remember *",
         "Did you think I would forget (2)?", "Why do you think I should recall (2) now?",
         "What about (2)?", "You mentioned (2)?"),
    RULE("dreamed", 4, false, "* i dreamed *",
         "Really, (2)?", "Have you ever fantasized (2) while you were awake?", "Have you dreamed (2) before?"),
    RULE("dream", 3, false, "*",
         "What does that dream suggest to you?", "Do you dream often?",
         "What persons appear in your dreams?", "Do you believe that dreams have something to do with your problem?"),
    RULE("want", 4, false, "* i want *",
         "What would it mean to you if you got (2)?", "Why do you want (2)?", "Suppose you got (2) soon.",
         "What if you never got (2)?", "What would getting (2) mean to you?",
         "What does wanting (2) have to do with this discussion?"),
    RULE("need", 4, false, "* i need *",
         "Why do you need (2)?", "Would it really help you to get (2)?", "Are you sure you need (2)?",
         "What would it mean if you had (2)?"),
    RULE("feel", 4, false, "* i feel *",
         "Tell me more about such feelings.", "Do you often feel (2)?", "Do you enjoy feeling (2)?",
         "Of what does feeling (2) remind you?"),
    RULE("sad", 4, false, "*",
         "I am sorry to hear you are not feeling well.", "Do you think coming here will help you?",
         "I'm sure it's not pleasant to feel that way.", "Can you explain what made you feel that way?"),
    RULE("happy", 3, false, "*",
         "How have I helped you to be happy?", "Has your treatment made you happy?",
         "What makes you happy just now?", "Can you explain why you are suddenly happy?"),
    RULE("love", 3, false, "* i love *",
         "What is it about (2) that you love?", "How long have you loved (2)?", "Does (2) know how you feel?"),
    RULE("hate", 3, false, "* i hate *",
         "What is it about (2) that bothers you?", "Has (2) always bothered you?",
         "What would change if (2) were different?"),
    RULE("think", 3, false, "* i think *",
         "Do you really think so?", "But you are not sure (2)?", "Do you doubt (2)?"),
    RULE("school", 3, false, "*",
         "How do you feel about school?", "What part of school is on your mind?",
         "Does school stress you out?", "Tell me more about school."),
    RULE("friend", 3, false, "*",
         "Why do you bring up the topic of friends?", "Do your friends worry you?",
         "Do your friends pick on you?", "Are you sure you have any friends?",
         "Perhaps your love for friends worries you."),
    RULE("family", 4, false, "*",
         "Tell me more about your family.", "Who else in your family comes to mind?",
         "How do you get along with your family?", "What else comes to mind when you think of your family?"),
    RULE("if", 3, false, "* if *",
         "Do you think it's likely that (2)?", "Do you wish that (2)?", "What do you know about (2)?",
         "Really, if (2)?"),
    RULE("everyone", 2, false, "*",
         "Really, everyone?", "Surely not everyone.", "Can you think of anyone in particular?",
         "Who, for example?", "Are you thinking of a very special person?", "Who, may I ask?",
         "Someone special perhaps?", "You have a particular person in mind, don't you?"),
    RULE("my", 2, true, "* my *",
         "Your (2)?", "Why do you say your (2)?", "Does that suggest anything else which belongs to you?",
         "Is it important to you that your (2)?"),
    RULE("was", 2, false, "* was i *",
         "What if you were (2)?", "Do you think you were (2)?", "Were you (2)?",
         "What would it mean if you were (2)?"),
    RULE("was", 2, false, "* i was *",
         "Were you really?", "Why do you tell me you were (2) now?", "Perhaps I already know you were (2)."),
    RULE("was", 2, false, "* was you *",
         "Would you like to believe I was (2)?", "What suggests that I was (2)?", "Perhaps I was (2).",
         "What if I had been (2)?"),
    RULE("like", 10, false, "* am * like *",
         "In what way?", "What resemblance do you see?", "What does that similarity suggest to you?",
         "What other connections do you see?"),
    RULE("like", 2, false, "* i like *",
         "Why do you like (2)?", "What do you like about (2)?", "Does liking (2) say something about you?"),
    RULE("like", 1, false, "*",
         "In what way?", "What resemblance do you see?", "What is the connection, do you suppose?",
         "Could there really be some connection?"),
    RULE("different", 1, false, "*",
         "How is it different?", "What differences do you see?", "What does that difference suggest to you?",
         "What other distinctions do you see?"),
    RULE("always", 1, false, "*",
         "Can you think of a specific example?", "When?", "What incident are you thinking of?",
         "Really, always?"),
    RULE("sorry", 1, false, "*",
         "Please don't apologise.", "Apologies are not necessary.",
         "What feelings do you have when you apologise?", "I've told you that apologies are not required."),
    RULE("perhaps", 1, false, "*",
         "You don't seem quite certain.", "Why the uncertain tone?", "Can't you be more positive?",
         "You aren't sure?", "Don't you know?"),
    RULE("call", 15, false, "* call me *",
         "Nice to meet you, (n).", "Hello, (n). What's on your mind?"),
    RULE("name", 16, false, "* my name is *",
         "Nice to meet you, (n).", "Hello, (n). What's on your mind?"),
    RULE("name", 16, false, "* my name *",
         "Your name is (n).", "You told me it's (n)."),
    RULE("name", 16, false, "* your name *",
         "I'm Eliza. What shall I call you?", "My name is Eliza. And yours?"),
    RULE("who", 5, false, "* who are you *",
         "I'm Eliza, a program running on a calculator.", "Just a program on a calculator. Who are you?"),
    RULE("what", 5, false, "* what are you *",
         "I'm Eliza, a program running on a calculator.", "A program on a calculator. What are you?"),
    RULE("robot", 5, false, "*",
         "I'm a program on a calculator. Does that bother you?", "Would you prefer to talk to a person?"),
    RULE("help", 2, false, "*",
         "I can chat, and I can do math. Try 'sin 30', '12*3' or '100 knots to mph'.",
         "What do you need help with?", "How can I help?"),
    RULE("game", 3, false, "*",
         "What do you like about games?", "Do games help you relax or stress you out?",
         "Who do you play with?", "What game is on your mind?"),
    RULE("fly", 3, false, "*",
         "What do you love about flying?", "Tell me about the first time you flew.",
         "Does flying help clear your head?", "What's the hardest part of learning to fly?",
         "How does it feel up there?"),
    RULE("phone", 2, false, "*",
         "Do you spend a lot of time on your phone?", "How do you feel after scrolling for a while?",
         "What do you usually watch?", "Does your phone ever get in the way of things you care about?"),
    RULE("tired", 4, false, "*",
         "Why do you think you're so tired?", "Are you getting enough sleep?",
         "What has been wearing you out?", "Is something keeping you up at night?"),
    RULE("bored", 3, false, "*",
         "What would you rather be doing?", "What usually makes you feel less bored?",
         "Is it boredom, or something else?"),
    RULE("mad", 4, false, "*",
         "What made you so angry?", "Does it help to talk about it?", "How long have you felt that way?",
         "Who is this about?"),
    RULE("worry", 4, false, "*",
         "What are you worried about?", "How long have you felt that way?",
         "What's the biggest thing on your mind right now?", "What would help you feel calmer?"),
    RULE("crush", 4, false, "*",
         "How do you feel about this person?", "Have you told them how you feel?",
         "What do you like about them?", "What are you afraid might happen?"),
    RULE("sport", 3, false, "*",
         "Do you play or just watch?", "What do you enjoy most about it?", "Who do you play with?"),
    RULE("music", 3, false, "*",
         "What kind of music do you like?", "Does music change how you feel?",
         "What song is stuck in your head?"),
    RULE("math", 3, false, "*",
         "How do you feel about math?", "Which part is giving you trouble?",
         "I can calculate things. Try 'sin 30' or '12*3'.", "Does math come easily to you?"),
    RULE("pet", 3, false, "*",
         "Tell me about your pet.", "How does your pet make you feel?", "Do you spend a lot of time together?"),
    RULE("money", 3, false, "*",
         "How do you feel about money?", "Is money worrying you?", "What would you do with more of it?"),
    RULE("job", 3, false, "*",
         "How do you feel about your work?", "Do you enjoy it?", "What would be your ideal job?"),
    RULE("birthday", 3, false, "*",
         "When is your birthday?", "Do you have plans for it?", "How do you feel about getting older?"),
    RULE("thanks", 1, false, "*",
         "You're welcome.", "Don't mention it.", "Any time."),
    RULE("lol", 1, false, "*",
         "What's so funny?", "You seem amused.", "Glad I could make you laugh."),
    RULE("please", 0, false, "*",
         "What would you like?", "How can I help?", "Tell me what you need."),
    RULE("how", 0, false, "how are you",
         "I'm doing fine. How about you?", "Fine, thanks for asking. How are you feeling?"),
    RULE("hello", 1, false, "*",
         "Hi. What's on your mind?", "Hello. What would you like to talk about?", "Hi there. How are you feeling today?"),
    RULE("can", 0, false, "* can you *",
         "You believe I can (2), don't you?", "You want me to be able to (2).",
         "Perhaps you would like to be able to (2) yourself."),
    RULE("can", 0, false, "* can i *",
         "Whether or not you can (2) depends on you more than on me.", "Do you want to be able to (2)?",
         "Perhaps you don't want to (2)."),
    RULE("why", 0, false, "* why dont you *",
         "Do you believe I don't (2)?", "Perhaps I will (2) in good time.", "Should you (2) yourself?",
         "You want me to (2)?"),
    RULE("why", 0, false, "* why cannot i *",
         "Do you think you should be able to (2)?", "Do you want to be able to (2)?",
         "Do you believe this will help you to (2)?", "Have you any idea why you can't (2)?"),
    RULE("why", 0, false, "*",
         "Why do you ask?", "Does that question interest you?", "What is it you really want to know?",
         "What do you think?", "What comes to mind when you ask that?"),
    RULE("what", 0, false, "*",
         "Why do you ask?", "Does that question interest you?", "What is it you really want to know?",
         "Are such questions much on your mind?", "What answer would please you most?",
         "What do you think?", "Have you asked such questions before?", "Have you asked anyone else?"),
    RULE("how", 0, false, "*",
         "Why do you ask?", "Does that question interest you?", "What is it you really want to know?",
         "Are such questions much on your mind?", "What do you think?"),
    RULE("because", 0, false, "*",
         "Is that the real reason?", "Don't any other reasons come to mind?",
         "Does that reason seem to explain anything else?", "What other reasons might there be?"),
    RULE("yes", 0, false, "*",
         "You seem quite positive.", "You are sure.", "I see.", "I understand."),
    RULE("no", 0, false, "*",
         "Are you saying no just to be negative?", "You are being a bit negative.", "Why not?", "Why no?"),
    RULE("am", 0, false, "* am i *",
         "Do you believe you are (2)?", "Would you want to be (2)?", "You wish I would tell you you are (2)?",
         "What would it mean if you were (2)?"),
    RULE("am", 0, false, "* i am *",
         "Is it because you are (2) that you came to me?", "How long have you been (2)?",
         "Do you believe it is normal to be (2)?", "Do you enjoy being (2)?",
         "Do you know anyone else who is (2)?"),
    RULE("are", 0, false, "* are you *",
         "Why are you interested in whether I am (2) or not?", "Would you prefer if I weren't (2)?",
         "Perhaps I am (2) in your fantasies.", "Do you sometimes think I am (2)?"),
    RULE("are", 0, false, "* you are *",
         "What makes you think I am (2)?", "Does it please you to believe I am (2)?",
         "Perhaps you would like to be (2).", "Do you sometimes wish you were (2)?"),
    RULE("are", 0, false, "* are *",
         "Did you think they might not be (2)?", "Would you like it if they were not (2)?",
         "What if they were not (2)?", "Possibly they are (2)."),
    RULE("your", 0, false, "* your *",
         "Why are you concerned over my (2)?", "What about your own (2)?",
         "Are you worried about someone else's (2)?", "Really, my (2)?"),
    RULE("you", 0, false, "* you remind me of *",
         "What makes you think of me?", "What resemblance do you see?",
         "What does that similarity suggest to you?", "What other connections do you see?"),
    RULE("you", 0, false, "* you * me *",
         "Why do you think I (2) you?", "You like to think I (2) you, don't you?",
         "What makes you think I (2) you?", "Really, I (2) you?", "Do you wish to believe I (2) you?",
         "Suppose I did (2) you. What would that mean?"),
    RULE("you", 0, false, "* you *",
         "We were discussing you, not me.", "Oh, I (2)?", "You're not really talking about me, are you?",
         "What are your feelings now?"),
    RULE("i", 0, false, "* i * you *",
         "Perhaps in your fantasies we (2) each other.", "Do you wish to (2) me?",
         "You seem to need to (2) me.", "Do you (2) anyone else?"),
    RULE("i", 0, false, "* i *",
         "You say you (2)?", "Can you elaborate on that?", "Do you say you (2) for some special reason?",
         "That's quite interesting.", "Tell me more."),
};

#define NUM_RULES ((int)(sizeof(rules) / sizeof(rules[0])))

static const char *const none_resp[] = {
    "I'm not sure I understand you fully.", "Please go on.", "What does that suggest to you?",
    "Do you feel strongly about discussing such things?", "That is interesting. Please continue.",
    "Tell me more about that.", "Does talking about this bother you?",
    "Go on, (n).", "(n), tell me more about that.",
};

static const char *const mem_resp[] = {
    "Earlier you said your (m).", "Does that have anything to do with the fact that your (m)?",
    "Let's discuss further why your (m).", "Is that why you mentioned your (m)?",
};

/* ---------- understanding messy input ---------- */

/* Words rewritten on input: abbreviations, slang, contractions, and synonyms
 * folded into the words the rules use. */
static const char *const expand[][2] = {
    {"i'm", "i am"}, {"im", "i am"}, {"you're", "you are"}, {"youre", "you are"},
    {"don't", "do not"}, {"dont", "do not"}, {"can't", "cannot"}, {"cant", "cannot"},
    {"won't", "will not"}, {"wont", "will not"}, {"i've", "i have"}, {"ive", "i have"},
    {"i'll", "i will"}, {"it's", "it is"}, {"that's", "that is"}, {"what's", "what is"},
    {"whats", "what is"}, {"thats", "that is"}, {"isn't", "is not"}, {"isnt", "is not"},
    {"didn't", "did not"}, {"didnt", "did not"}, {"doesn't", "does not"}, {"doesnt", "does not"},
    {"u", "you"}, {"ur", "your"}, {"r", "are"}, {"y", "why"}, {"n", "and"}, {"&", "and"},
    {"k", "yes"}, {"kk", "yes"}, {"ok", "yes"}, {"okay", "yes"}, {"yeah", "yes"}, {"yea", "yes"},
    {"yep", "yes"}, {"yup", "yes"}, {"ya", "you"}, {"nope", "no"}, {"nah", "no"},
    {"luv", "love"}, {"pls", "please"}, {"plz", "please"}, {"thx", "thanks"}, {"ty", "thanks"}, {"thanx", "thanks"},
    {"lmao", "lol"}, {"lmfao", "lol"}, {"haha", "lol"}, {"hehe", "lol"}, {"omg", "wow"},
    {"idk", "i do not know"}, {"idc", "i do not care"}, {"imo", "in my opinion"},
    {"tbh", "to be honest"}, {"btw", "by the way"}, {"rn", "right now"}, {"ppl", "people"},
    {"bc", "because"}, {"cuz", "because"}, {"cos", "because"}, {"coz", "because"},
    {"cus", "because"}, {"bcuz", "because"}, {"becuz", "because"}, {"b4", "before"},
    {"gr8", "great"}, {"l8r", "later"}, {"l8", "late"}, {"w8", "wait"}, {"h8", "hate"},
    {"2day", "today"}, {"2moro", "tomorrow"}, {"2morrow", "tomorrow"}, {"tmrw", "tomorrow"},
    {"tmr", "tomorrow"}, {"2nite", "tonight"}, {"any1", "anyone"}, {"some1", "someone"},
    {"every1", "everyone"}, {"w/", "with"}, {"w/o", "without"}, {"wanna", "want to"},
    {"gonna", "going to"}, {"gotta", "have to"}, {"kinda", "kind of"}, {"dunno", "do not know"},
    {"lemme", "let me"}, {"gimme", "give me"}, {"tho", "though"}, {"wat", "what"}, {"wut", "what"},
    {"wen", "when"}, {"hw", "homework"}, {"bf", "boyfriend"}, {"gf", "girlfriend"}, {"teh", "the"}, {"adn", "and"}, {"taht", "that"},
    {"hte", "the"}, {"jsut", "just"}, {"abotu", "about"}, {"thier", "their"}, {"freind", "friend"},
    {"hi", "hello"}, {"hey", "hello"}, {"hiya", "hello"}, {"howdy", "hello"}, {"sup", "hello"},
    {"goodbye", "bye"}, {"cya", "bye"}, {"gtg", "bye"}, {"ttyl", "bye"},
    {"dreams", "dream"}, {"dreamt", "dreamed"},
    };

#define NUM_EXPAND ((int)(sizeof(expand) / sizeof(expand[0])))

/* Topic words: the word is kept as typed (so echoes sound right) but also
 * counts as a keyword for the rules. */
static const char *const alias[][2] = {
    {"everybody", "everyone"}, {"nobody", "everyone"}, {"noone", "everyone"},
    {"computers", "computer"}, {"machine", "computer"}, {"machines", "computer"},
    {"depressed", "sad"}, {"unhappy", "sad"}, {"lonely", "sad"}, {"upset", "sad"}, {"glad", "happy"},
    {"homework", "school"}, {"teacher", "school"}, {"class", "school"}, {"test", "school"},
    {"exam", "school"}, {"alike", "like"}, {"same", "like"}, {"friends", "friend"}, {"bestie", "friend"},
    {"mom", "family"}, {"dad", "family"}, {"mother", "family"}, {"father", "family"},
    {"parents", "family"}, {"brother", "family"}, {"sister", "family"},
    {"games", "game"}, {"gaming", "game"}, {"gamer", "game"}, {"minecraft", "game"}, {"roblox", "game"},
    {"fortnite", "game"}, {"xbox", "game"}, {"playstation", "game"},
    {"glider", "fly"}, {"gliders", "fly"}, {"plane", "fly"}, {"planes", "fly"}, {"flying", "fly"},
    {"flew", "fly"}, {"pilot", "fly"}, {"airplane", "fly"}, {"flight", "fly"}, {"soaring", "fly"},
    {"tiktok", "phone"}, {"instagram", "phone"}, {"youtube", "phone"}, {"snapchat", "phone"},
    {"sleepy", "tired"}, {"exhausted", "tired"}, {"sleep", "tired"},
    {"boring", "bored"}, {"angry", "mad"}, {"annoyed", "mad"}, {"furious", "mad"},
    {"scared", "worry"}, {"afraid", "worry"}, {"nervous", "worry"}, {"anxious", "worry"},
    {"worried", "worry"}, {"anxiety", "worry"}, {"stressed", "worry"}, {"stress", "worry"},
    {"crush", "crush"}, {"girlfriend", "crush"}, {"boyfriend", "crush"},
    {"soccer", "sport"}, {"football", "sport"}, {"basketball", "sport"}, {"baseball", "sport"},
    {"tennis", "sport"}, {"sports", "sport"}, {"songs", "music"}, {"song", "music"},
    {"guitar", "music"}, {"piano", "music"}, {"trig", "math"}, {"algebra", "math"},
    {"geometry", "math"}, {"calculus", "math"}, {"sine", "math"}, {"cosine", "math"},
    {"cats", "pet"}, {"dogs", "pet"}, {"dog", "pet"}, {"cat", "pet"}, {"puppy", "pet"}, {"kitten", "pet"},
    {"jobs", "job"}, {"broke", "money"}, {"human", "robot"}, {"bot", "robot"},
};

#define NUM_ALIAS ((int)(sizeof(alias) / sizeof(alias[0])))

#define MAX_TAGS 16
static char tags[MAX_TAGS][WORD_LEN];
static int ntags;
static char user_name[WORD_LEN];

static void add_tag(const char *t)
{
    int i;
    for (i = 0; i < ntags; i++) {
        if (!strcmp(tags[i], t)) {
            return;
        }
    }
    if (ntags < MAX_TAGS) {
        strcpy(tags[ntags++], t);
    }
}

/* Words the rules care about. A misspelled word close to one of these is
 * corrected to it (longer words only, so real words are not mangled). */
static const char *const targets[] = {
    "remember", "dreamed", "dream", "computer", "computers", "because", "sorry", "perhaps",
    "hello", "family", "mother", "father", "brother", "sister", "parents", "friend", "friends",
    "school", "homework", "teacher", "think", "feel", "want", "need", "love", "hate", "happy",
    "depressed", "unhappy", "lonely", "everyone", "everybody", "always", "different", "goodbye",
    "thanks", "please", "name", "sad", "like", "what", "when", "where", "why", "how", "can",
    "minecraft", "youtube", "airplane", "worried", "nervous", "anxious", "scared", "afraid",
    "exhausted", "glider", "gliders", "tiktok", "instagram", "birthday", "boyfriend", "girlfriend",
    "calculus", "geometry", "algebra", "basketball", "football", "soccer", "music", "pilot",
    "flying", "tired", "bored", "angry", "annoyed", "crush", "money", "sports", "stress", "stressed",
    "roblox", "fortnite", "snapchat", "everyone", "nobody", "family", "mom", "dad", "help",
    "boyle", "charles", "avogadro", "dalton", "graham", "kelvin", "pressure", "volume",
    "temperature", "molecule", "molecules", "ideal", "effusion", "diffusion", "barometer",
    "manometer", "pythagorean", "identity", "identities", "sine", "cosine", "tangent", "secant",
    "cosecant", "cotangent", "triangle", "ambiguous", "proof", "prove", "reciprocal", "quotient",
    "conjugate", "hypotenuse", "amplitude", "period", "radians", "degrees", "elevation",
    "depression", "resultant", "vapor", "stoichiometry", "combined", "kinetic", "molar", "mole",
    "formula", "explain", "equation", "problem", "example", "practice", "gases",
};

/* Ordinary words that are never "corrected" into something else. */
static const char *const common[] = {
    "i", "me", "my", "you", "your", "he", "she", "it", "we", "they", "them", "us", "him", "her",
    "his", "hers", "its", "our", "their", "is", "are", "was", "were", "be", "been", "am", "do",
    "does", "did", "have", "has", "had", "will", "would", "could", "should", "may", "might",
    "must", "a", "an", "the", "and", "or", "but", "if", "so", "as", "at", "by", "for", "in",
    "of", "on", "to", "with", "from", "about", "into", "than", "then", "that", "this", "these",
    "those", "there", "here", "who", "which", "not", "no", "yes", "too", "very", "really",
    "just", "also", "only", "even", "still", "again", "never", "ever", "more", "most", "much",
    "many", "some", "any", "all", "each", "every", "other", "another", "such", "own", "few",
    "little", "big", "small", "good", "bad", "new", "old", "long", "great", "right", "wrong",
    "first", "last", "next", "day", "time", "year", "week", "today", "tomorrow", "night",
    "morning", "life", "world", "people", "thing", "things", "thing", "man", "woman", "child",
    "kids", "home", "house", "work", "job", "game", "games", "play", "make", "made", "get",
    "got", "go", "going", "went", "come", "came", "see", "saw", "look", "say", "said", "tell",
    "told", "ask", "know", "knew", "thought", "felt", "help", "try", "use", "find", "give",
    "take", "put", "keep", "let", "begin", "seem", "leave", "call", "talk", "turn", "start",
    "show", "hear", "run", "live", "lives", "move", "believe", "bring", "happen", "write",
    "sit", "stand", "lose", "lost", "pay", "meet", "learn", "change", "watch", "stop", "speak",
    "read", "walk", "win", "buy", "wait", "send", "stay", "fall", "late", "date", "line",
    "mine", "hope", "hear", "head", "hand", "face", "eyes", "mean", "means", "meant", "tired",
    "bored", "angry", "scared", "afraid", "worried", "stressed", "anxious", "excited", "nervous",
    "thank", "thanks", "sure", "maybe", "okay", "well", "back", "down", "over", "after", "before",
    "while", "since", "until", "though", "through", "because", "something", "anything", "nothing",
    "everything", "someone", "anyone", "everyone", "myself", "yourself", "pizza", "movie",
    "music", "phone", "money", "money", "food", "water", "dog", "cat", "car", "book", "story",
    "dinner", "lunch", "breakfast", "sleep", "party", "lol", "wow", "cannot", "bye",
};

#define N_TARGETS ((int)(sizeof(targets) / sizeof(targets[0])))
#define N_COMMON ((int)(sizeof(common) / sizeof(common[0])))

static bool in_list(const char *const *list, int n, const char *w)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(list[i], w)) {
            return true;
        }
    }
    return false;
}

/* Edit distance counting an adjacent swap as one edit. */
static int edit_distance(const char *a, const char *b)
{
    uint8_t d[WORD_LEN + 1][WORD_LEN + 1];
    int la = (int)strlen(a), lb = (int)strlen(b);
    int i, j;

    for (i = 0; i <= la; i++) d[i][0] = (uint8_t)i;
    for (j = 0; j <= lb; j++) d[0][j] = (uint8_t)j;
    for (i = 1; i <= la; i++) {
        for (j = 1; j <= lb; j++) {
            int cost = a[i - 1] != b[j - 1];
            int v = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1;
            if (d[i - 1][j - 1] + cost < v) v = d[i - 1][j - 1] + cost;
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] && d[i - 2][j - 2] + 1 < v) {
                v = d[i - 2][j - 2] + 1;
            }
            d[i][j] = (uint8_t)v;
        }
    }
    return d[la][lb];
}

static bool is_swap(const char *a, const char *b)
{
    int i, n = (int)strlen(a);
    if (n != (int)strlen(b)) {
        return false;
    }
    for (i = 0; i + 1 < n; i++) {
        if (a[i] != b[i]) {
            return a[i] == b[i + 1] && a[i + 1] == b[i] && !strcmp(a + i + 2, b + i + 2);
        }
    }
    return false;
}

/* Closest word the rules know, or NULL. */
static const char *spell_fix(const char *w)
{
    int len = (int)strlen(w);
    int best = 99, i;
    const char *found = NULL;

    if (len < 4) {
        return NULL;
    }
    for (i = 0; i < N_TARGETS; i++) {
        const char *cand = targets[i];
        int clen = (int)strlen(cand);
        int limit = len >= 8 ? 2 : 1;
        int d;

        if (clen < 4 || clen > len + limit || clen < len - limit) {
            continue;
        }
        d = edit_distance(w, cand);
        if (len == 4 ? !(is_swap(w, cand) || (clen >= 5 && d <= 1)) : d > limit) {
            continue;
        }
        if (d < best) {
            best = d;
            found = cand;
        }
    }
    return found;
}

static void push_word(const char *w)
{
    if (nw < MAX_WORDS) {
        strncpy(words[nw], w, WORD_LEN - 1);
        words[nw][WORD_LEN - 1] = 0;
        nw++;
    }
}

/* Push the words of an expansion string such as "i do not know". */
static void push_phrase(const char *r)
{
    char one[WORD_LEN];
    int n = 0;
    for (;; r++) {
        if (*r == ' ' || !*r) {
            one[n] = 0;
            if (n) {
                push_word(one);
            }
            n = 0;
            if (!*r) {
                break;
            }
        } else if (n < WORD_LEN - 1) {
            one[n++] = *r;
        }
    }
}

static const char *expansion_of(const char *w)
{
    int i;
    for (i = 0; i < NUM_EXPAND; i++) {
        if (!strcmp(w, expand[i][0])) {
            return expand[i][1];
        }
    }
    return NULL;
}

static void add_token(const char *w)
{
    const char *r = expansion_of(w);

    if (!r && !in_list(targets, N_TARGETS, w) && !in_list(common, N_COMMON, w)) {
        const char *fixed = spell_fix(w);
        if (fixed) {
            w = fixed;
            r = expansion_of(w);
        }
    }
    if (r) {
        push_phrase(r);
    } else {
        push_word(w);
    }
}

static char raw[MAX_WORDS][WORD_LEN];
static int nraw;

static bool all_digits(const char *w)
{
    for (; *w; w++) {
        if (*w < '0' || *w > '9') {
            return false;
        }
    }
    return true;
}

static bool is_pronoun(const char *w)
{
    static const char *const p[] = {"me", "you", "i", "it", "us", "them", "he", "she", "we", "they", "u", "and"};
    return in_list(p, (int)(sizeof(p) / sizeof(p[0])), w);
}

/* "2" and "4" are numbers ("2 dogs") or words ("want 2 go", "4 you", "me 2"). */
static const char *digit_word(int i)
{
    const char *w = raw[i];
    const char *prev = i > 0 ? raw[i - 1] : NULL;
    const char *next = i + 1 < nraw ? raw[i + 1] : NULL;
    bool two = w[0] == '2';
    int len;

    if (!next) {
        return (two && prev && is_pronoun(prev)) ? "too" : w;
    }
    if (all_digits(next) || !strcmp(next, "people") || !strcmp(next, "men") || !strcmp(next, "women")
        || !strcmp(next, "children") || !strcmp(next, "feet") || !strcmp(next, "dozen")) {
        return w;
    }
    len = (int)strlen(next);
    if (len > 3 && next[len - 1] == 's' && next[len - 2] != 's') {
        return w;
    }
    return two ? "to" : "for";
}

static void tokenize(const char *s)
{
    char cur[WORD_LEN];
    int n = 0, run = 0, i;
    char last = 0;

    nraw = 0;
    nw = 0;
    for (;; s++) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') {
            c += 32;
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '/' || c == '&') {
            run = c == last ? run + 1 : 1;
            last = c;
            if (n < WORD_LEN - 1 && !(c >= 'a' && c <= 'z' && run > 2)) {
                cur[n++] = c;
            }
        } else {
            if (n && nraw < MAX_WORDS) {
                cur[n] = 0;
                /* "boyle's" -> "boyle" (but keep contractions such as "what's") */
                if (n > 2 && cur[n - 2] == '\'' && cur[n - 1] == 's' && !expansion_of(cur)) {
                    cur[n - 2] = 0;
                }
                strcpy(raw[nraw++], cur);
            }
            n = 0;
            last = 0;
            run = 0;
            if (!c) {
                break;
            }
        }
    }
    for (i = 0; i < nraw; i++) {
        if (!strcmp(raw[i], "2") || !strcmp(raw[i], "4")) {
            add_token(digit_word(i));
        } else {
            add_token(raw[i]);
        }
    }
    ntags = 0;
    for (i = 0; i < nw; i++) {
        int a;
        for (a = 0; a < NUM_ALIAS; a++) {
            if (!strcmp(words[i], alias[a][0])) {
                add_tag(alias[a][1]);
            }
        }
    }
}

/* ---------- pattern matching ---------- */

static char pat_buf[48];
static const char *pat_tok[MAX_PAT];
static int pat_n;

static void split_pattern(const char *pat)
{
    char *c;
    strncpy(pat_buf, pat, sizeof(pat_buf) - 1);
    pat_buf[sizeof(pat_buf) - 1] = 0;
    pat_n = 0;
    c = pat_buf;
    while (*c && pat_n < MAX_PAT) {
        pat_tok[pat_n++] = c;
        while (*c && *c != ' ') {
            c++;
        }
        if (*c) {
            *c++ = 0;
        }
    }
}

static bool match_from(int pi, int wi, int cap)
{
    if (pi == pat_n) {
        return wi == nw;
    }
    if (!strcmp(pat_tok[pi], "*")) {
        int len;
        for (len = 0; wi + len <= nw; len++) {
            cap_start[cap] = wi;
            cap_end[cap] = wi + len;
            if (match_from(pi + 1, wi + len, cap + 1)) {
                return true;
            }
        }
        return false;
    }
    if (wi < nw && !strcmp(pat_tok[pi], words[wi])) {
        return match_from(pi + 1, wi + 1, cap);
    }
    return false;
}

static bool has_word(const char *key)
{
    int i;
    for (i = 0; i < nw; i++) {
        if (!strcmp(words[i], key)) {
            return true;
        }
    }
    for (i = 0; i < ntags; i++) {
        if (!strcmp(tags[i], key)) {
            return true;
        }
    }
    return false;
}

/* ---------- building the reply ---------- */

static bool is_verb(const char *w)
{
    static const char *const verbs[] = {
        "are", "were", "will", "would", "have", "do", "did", "can", "could", "should", "must",
        "may", "might", "am", "cannot", "think", "know", "like", "love", "hate", "want", "need",
    };
    int i;
    for (i = 0; i < (int)(sizeof(verbs) / sizeof(verbs[0])); i++) {
        if (!strcmp(w, verbs[i])) {
            return true;
        }
    }
    return false;
}

/* Append words[from..to) with I/you swapped, so "i am sad" echoes as "you are sad". */
static void put_reflected(int from, int to)
{
    int k;
    for (k = from; k < to; k++) {
        const char *w = words[k];
        const char *prev = k > from ? words[k - 1] : NULL;
        const char *next = k + 1 < to ? words[k + 1] : NULL;
        const char *rep = w;

        if (!strcmp(w, "i") || !strcmp(w, "me")) rep = "you";
        else if (!strcmp(w, "my")) rep = "your";
        else if (!strcmp(w, "myself")) rep = "yourself";
        else if (!strcmp(w, "mine")) rep = "yours";
        else if (!strcmp(w, "am")) rep = "are";
        else if (!strcmp(w, "your")) rep = "my";
        else if (!strcmp(w, "yours")) rep = "mine";
        else if (!strcmp(w, "yourself")) rep = "myself";
        else if (!strcmp(w, "you")) rep = (next && is_verb(next)) ? "I" : "me";
        else if (!strcmp(w, "are") && prev && !strcmp(prev, "you")) rep = "am";
        else if (!strcmp(w, "was") && prev && !strcmp(prev, "i")) rep = "were";

        if (k > from) {
            put_str(" ");
        }
        put_str(rep);
    }
}

/* A template is usable only if every (n) it uses has something to fill it. */
static bool template_ok(const char *tpl)
{
    for (; *tpl; tpl++) {
        if (tpl[0] == '(' && tpl[2] == ')') {
            if (tpl[1] == 'm' && !memory[0]) {
                return false;
            }
            if (tpl[1] == 'n' && !user_name[0]) {
                return false;
            }
            if (tpl[1] >= '1' && tpl[1] <= '9' && cap_start[tpl[1] - '1'] == cap_end[tpl[1] - '1']) {
                return false;
            }
        }
    }
    return true;
}

static void build(const char *tpl, const char *key)
{
    wp = out;
    wend = out + OUT_LEN - 1;
    for (; *tpl; tpl++) {
        if (tpl[0] == '(' && tpl[2] == ')' && ((tpl[1] >= '0' && tpl[1] <= '9') || tpl[1] == 'm' || tpl[1] == 'n')) {
            if (tpl[1] == 'n') {
                put_str(user_name);
            } else if (tpl[1] == 'm') {
                put_str(memory);
            } else if (tpl[1] == '0') {
                put_str(key);
            } else {
                int c = tpl[1] - '1';
                put_reflected(cap_start[c], cap_end[c]);
            }
            tpl += 2;
        } else if (wp < wend) {
            *wp++ = *tpl;
        }
    }
    *wp = 0;
    if (out[0] >= 'a' && out[0] <= 'z') {
        out[0] -= 32;
    }
}

/* "my name is sam" / "call me sam": keep the first word of what follows. */
static void maybe_set_name(const struct rule *r)
{
    int stars = 0, i;
    int from, to;

    if (!((!strcmp(r->key, "name") && strstr(r->pat, "name is")) || (!strcmp(r->key, "call") && strstr(r->pat, "call me")))) {
        return;
    }
    for (i = 0; r->pat[i]; i++) {
        if (r->pat[i] == '*') {
            stars++;
        }
    }
    from = cap_start[stars - 1];
    to = cap_end[stars - 1];
    if (to > from && to - from <= 2) {
        strncpy(user_name, words[from], WORD_LEN - 1);
        user_name[WORD_LEN - 1] = 0;
        if (user_name[0] >= 'a' && user_name[0] <= 'z') {
            user_name[0] -= 32;
        }
    }
}

static void remember(const struct rule *r)
{
    int last = 0;
    int i;

    if (!r->mem || have_mem) {
        return;
    }
    for (i = 0; r->pat[i]; i++) {
        if (r->pat[i] == '*') {
            last++;
        }
    }
    if (last < 1) {
        return;
    }
    wp = memory;
    wend = memory + sizeof(memory) - 1;
    put_reflected(cap_start[last - 1], cap_end[last - 1]);
    *wp = 0;
    have_mem = memory[0] != 0;
}

int eliza_tokens(const char *input, char out[][ELIZA_WORD_LEN], int max)
{
    int n = 0, i;

    tokenize(input);
    for (i = 0; i < nw && n < max; i++) {
        strcpy(out[n++], words[i]);
    }
    for (i = 0; i < ntags && n < max; i++) {
        strcpy(out[n++], tags[i]);
    }
    /* the words as typed too, so "proof 2" still matches the digit 2 */
    for (i = 0; i < nraw && n < max; i++) {
        strcpy(out[n++], raw[i]);
    }
    return n;
}

const char *eliza_reply(const char *input)
{
    int rank, i;

    if (calc_try(input, out, OUT_LEN)) {
        return out;
    }
    tokenize(input);
    if (!nw) {
        return "Say something, please.";
    }
    for (rank = 60; rank >= 0; rank--) {
        for (i = 0; i < NUM_RULES; i++) {
            struct rule *r = &rules[i];
            if (r->rank != rank || !has_word(r->key)) {
                continue;
            }
            split_pattern(r->pat);
            if (match_from(0, 0, 0)) {
                int t;
                maybe_set_name(r);
                for (t = 0; t < r->n; t++) {
                    int idx = (r->next + t) % r->n;
                    if (template_ok(r->resp[idx])) {
                        build(r->resp[idx], r->key);
                        r->next = (uint8_t)((idx + 1) % r->n);
                        remember(r);
                        return out;
                    }
                }
            }
        }
    }
    if (have_mem) {
        have_mem = false;
        build(mem_resp[mem_next++ % (sizeof(mem_resp) / sizeof(mem_resp[0]))], "");
        return out;
    }
    {
        int t;
        for (t = 0; t < 8; t++) {
            const char *tpl = none_resp[none_next++ % (sizeof(none_resp) / sizeof(none_resp[0]))];
            if (template_ok(tpl)) {
                build(tpl, "");
                return out;
            }
        }
    }
    build("Please go on.", "");
    return out;
}
