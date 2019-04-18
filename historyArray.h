struct historyArray {
    int firstItem;
    int lastItem;
    int size;
    char** items;
} historyArray_t;

struct historyArray* createHistoryArray();
void removeHistoryArray(struct historyArray* ha);
void addToHistoryArray(struct historyArray* ha, char** commandsToAdd, _Bool in_background);