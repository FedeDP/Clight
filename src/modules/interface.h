struct prop_cb {
    const char *name;
    void (*cb)(void);
};

int emit_prop(const char *signal);
int add_prop_callback(struct prop_cb *cb);
