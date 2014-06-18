#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "Counted.h"
#include <list>

// ****************************************************************************
// Context used for interprocedural analysis
// ****************************************************************************
class Context : public utils::pt::Counted {
protected:

public:
    Context() {};
    virtual ~Context() {};
    virtual const std::string tostring() = 0;
    virtual bool operator<(const Context &) = 0;
};


class ContextInsensitive : public Context {
private:
    addr_t context;
public:
    ContextInsensitive(addr_t a) { context = a;} ;
    ContextInsensitive(const Context &ctx, addr_t a) { 
	(void)ctx;
	context = a; 
    }

    const std::string tostring() {
	char tmp[16];
	sprintf(tmp, "%.8x", context);
	return tmp;
    }

    bool operator<(const Context &o_) {
	const ContextInsensitive &o = dynamic_cast<const ContextInsensitive &>(o_);
	return context < o.context;
    }
};

class ContextSensitive : public Context {
private:
    std::list<addr_t> context;
public:
    ContextSensitive(addr_t a) { context.push_back(a); }
    ContextSensitive(const Context &, addr_t);
    const std::string tostring();

    bool operator<(const Context &o_) {
	const ContextSensitive &o = dynamic_cast<const ContextSensitive &>(o_);
	if (context.size() != o.context.size())
	    return context.size() < o.context.size();

	std::list<addr_t>::const_reverse_iterator i1 = context.rbegin(), 
	    e1 = context.rend(), i2 = o.context.rbegin(), e2 = o.context.rend();
	for (; i1 != e1 && i2 != e2; i1++, i2++) {
	    if (*i1 != *i2) {
		return *i1 < *i2;
	    }
	}
	return false;
    }
};

ContextSensitive::ContextSensitive(const Context &ctx_, addr_t addr) {
    const ContextSensitive *ctx = dynamic_cast<const ContextSensitive *>(&ctx_);
    assert(ctx);

    context.insert(context.begin(), ctx->context.begin(), ctx->context.end());
    context.push_back(addr);
}

const std::string ContextSensitive::tostring() {
    std::string r = "";

    for (std::list<addr_t>::const_iterator it = context.begin();
	 it != context.end(); it++) {
	char tmp[16];
	sprintf(tmp, "%s%.8x", it != context.begin() ? " " : "", *it);
	r += tmp;
    }

    return r;
}

class KContextSensitive : public Context {
private:
    unsigned int k;
    std::list<addr_t> context;

public:
    KContextSensitive(unsigned int k_ = (unsigned int) 3) { k = k_;};
    KContextSensitive(addr_t, unsigned int k_ = (unsigned int) 3);
    KContextSensitive(const Context &, addr_t);
    const std::string tostring();
    
    bool operator<(const Context &o_) {
	const KContextSensitive &o = dynamic_cast<const KContextSensitive &>(o_);

	if (context.size() != o.context.size())
	    return context.size() < o.context.size();

	std::list<addr_t>::const_reverse_iterator i1 = context.rbegin(), 
	    e1 = context.rend(), i2 = o.context.rbegin(), e2 = o.context.rend();
	for (; i1 != e1 && i2 != e2; i1++, i2++) {
	    if (*i1 != *i2) {
		return *i1 < *i2;
	    }
	}
	return false;
    }
};

KContextSensitive::KContextSensitive(addr_t addr, unsigned int k_) {
    k = k_;
    context.push_back(addr);
}

KContextSensitive::KContextSensitive(const Context &ctx_, addr_t addr) {
    const KContextSensitive *ctx = dynamic_cast<const KContextSensitive *>(&ctx_);
    assert(ctx);

    unsigned int j;

    k = ctx->k;
    j = k - 1;

    for (std::list<addr_t>::const_iterator it = ctx->context.begin();
	 it != ctx->context.end(), j > 0; it++, j--) {
	context.push_front(*it);
    }
    context.push_back(addr);
    assert(context.size() <= k);

}

const std::string KContextSensitive::tostring() {
    std::string r = "";

    for (std::list<addr_t>::const_iterator it = context.begin();
	 it != context.end(); it++) {
	char tmp[16];
	sprintf(tmp, "%s%.8x", it != context.begin() ? " " : "", *it);
	r += tmp;
    }

    return r;
}

typedef boost::intrusive_ptr<Context> ContextPtr;

// Compare is a Strict Weak Ordering whose argument type is Key. 
struct ctxcmp {
  bool operator()(const ContextPtr ctx1, const ContextPtr ctx2) const {
      return *ctx1 < *ctx2;
  }
};

#endif // !__CONTEXT_H__


// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
