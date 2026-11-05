module;

export module app;

import :detail;
export import :api;

module :private;

int app_private_only() {
    return 9;
}
