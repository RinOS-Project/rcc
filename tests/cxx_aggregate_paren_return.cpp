int returned_aggregate_destructions;
int returned_aggregate_sum;

struct ReturnedAggregate {
    int first;
    int second;

    ~ReturnedAggregate() {
        ++returned_aggregate_destructions;
        returned_aggregate_sum += first + second;
    }
};

static ReturnedAggregate make_returned_aggregate(int first, int second) {
    return ReturnedAggregate(first, second);
}

int main() {
    int result = 0;
    {
        ReturnedAggregate value = make_returned_aggregate(13, 17);
        if (value.first != 13 || value.second != 17) {
            result = 1;
        }
    }
    return result == 0 && returned_aggregate_destructions == 1 &&
                   returned_aggregate_sum == 30
               ? 0
               : 2;
}
