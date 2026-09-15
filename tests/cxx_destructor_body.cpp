struct Tracked {
    int* count;

    Tracked(int* value) : count(value) {}

    ~Tracked() {
        *count += 1;
    }
};

int local_destructor() {
    int count = 0;
    {
        Tracked value(&count);
    }
    return count == 1 ? 0 : 1;
}

int heap_destructor() {
    int count = 0;
    Tracked* value = new Tracked(&count);
    delete value;
    return count == 1 ? 0 : 1;
}

int main() {
    return local_destructor() + heap_destructor();
}
