#define ADD_PROP_CB(cb) (cb)->dst = self.idx; add_prop_callback(cb);

struct prop_cb {
    const char *name;
    void (*cb)(void);
    enum modules dst;
};

int emit_prop(const char *signal);
int emit_mod_prop(const char *signal);
int add_prop_callback(struct prop_cb *cb);
