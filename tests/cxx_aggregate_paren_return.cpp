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

static ReturnedAggregate select_returned_aggregate(
    bool use_temporary, const ReturnedAggregate& source) {
    return use_temporary ? ReturnedAggregate(31, 37) : source;
}

int main() {
    int result = 0;
    {
        ReturnedAggregate value = make_returned_aggregate(13, 17);
        if (value.first != 13 || value.second != 17) {
            result = 1;
        }
    }
    if (result != 0 || returned_aggregate_destructions != 1 ||
        returned_aggregate_sum != 30) {
        return 2;
    }

    {
        ReturnedAggregate source = make_returned_aggregate(23, 29);
        {
            ReturnedAggregate selected =
                select_returned_aggregate(false, source);
            if (selected.first != 23 || selected.second != 29) return 3;
        }
        if (returned_aggregate_destructions != 2 ||
            returned_aggregate_sum != 82) {
            return 4;
        }

        {
            ReturnedAggregate selected =
                select_returned_aggregate(true, source);
            if (selected.first != 31 || selected.second != 37) return 5;
        }
        if (returned_aggregate_destructions != 3 ||
            returned_aggregate_sum != 150) {
            return 6;
        }
    }
    return returned_aggregate_destructions == 4 &&
                   returned_aggregate_sum == 202
               ? 0
               : 7;
}
