// Sorting and searching with qsort and bsearch: a comparison function, numbers, strings and records.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static int by_value(const void* a, const void* b)
{
    int x = *(const int*)a, y = *(const int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static int by_value_desc(const void* a, const void* b)
{
    return by_value(b, a);
}

static int by_text(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

struct person { const char* name; int age; };

// oldest first; the same age: alphabetical
static int by_age_then_name(const void* a, const void* b)
{
    const struct person* p = (const struct person*)a;
    const struct person* q = (const struct person*)b;
    if (p->age != q->age)
        return q->age - p->age;
    return strcmp(p->name, q->name);
}

static void print_ints(const char* label, const int* v, int n)
{
    printf("%-10s", label);
    for (int i = 0; i < n; i++)
        printf(" %d", v[i]);
    putchar('\n');
}

int main(void)
{
    int v[] = { 42, 7, 19, 3, 88, 7, 25, 1 };
    int n = sizeof v / sizeof v[0];
    print_ints("as given:", v, n);
    qsort(v, n, sizeof v[0], by_value);
    print_ints("ascending:", v, n);
    qsort(v, n, sizeof v[0], by_value_desc);
    print_ints("descending:", v, n);
    qsort(v, n, sizeof v[0], by_value);

    int wanted[] = { 19, 20, 88, 0 };
    for (int i = 0; i < 4; i++)
    {
        int* hit = (int*)bsearch(&wanted[i], v, n, sizeof v[0], by_value);
        if (hit != NULL)
            printf("%d is at index %d\n", wanted[i], (int)(hit - v));
        else
            printf("%d is not there\n", wanted[i]);
    }

    const char* words[] = { "pear", "apple", "fig", "cherry", "banana" };
    qsort(words, 5, sizeof words[0], by_text);
    printf("words:");
    for (int i = 0; i < 5; i++)
        printf(" %s", words[i]);
    putchar('\n');

    struct person people[] = { { "Ana", 31 }, { "Luis", 45 }, { "Marta", 31 }, { "Iker", 45 }, { "Bea", 28 } };
    qsort(people, 5, sizeof people[0], by_age_then_name);
    for (int i = 0; i < 5; i++)
        printf("%-6s %d\n", people[i].name, people[i].age);
    return 0;
}
