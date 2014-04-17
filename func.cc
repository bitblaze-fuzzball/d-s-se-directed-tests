#include "func.h"
#include "Utilities.h"

Function::Function(std::string n, addr_t a, size_t l, std::string m) {
    name = n;
    address = a;
    size = l;
    module = m;
    cfg = new Cfg(this);
    argumentsno = -1;
    pending = false;
    prog = NULL;
}

Function::Function(addr_t a) {
    argumentsno = -1; 
    cfg = new  Cfg(this);
    address = a; 
    name = "unknown"; 
    module = "unknown";
    pending = false;
    prog = NULL;
}

Function::~Function() {
    delete cfg;
}

const char *Function::getName() const {
    return name.c_str();
}

const char *Function::getModule() const {
    return module.c_str();
}

addr_t Function::getAddress() const {
    return address;
}

size_t Function::getSize() const {
    return size;
}

Cfg *Function::getCfg() {
    return cfg;
}

// Scan the instructions to detect the number of arguments (assume the
// framepointer is not used as a general purpose register,
// -fno-omit-frame-pointer)
void Function::guessArgumentsNo() {
    cfg->decode();

    for (Cfg::const_bb_iterator bbit = cfg->bb_begin();
	 bbit != cfg->bb_end(); bbit++) {
	for (instructions_t::const_iterator iit = (*bbit)->inst_begin();
	     iit != (*bbit)->inst_end(); iit++) {
	    std::string tempebp;

	    for (statements_t::const_iterator sit = (*iit)->stmt_begin();
		 sit != (*iit)->stmt_end(); sit++) {
		vine::Stmt *s = *sit;

		if (s->stmt_type == vine::MOVE) {
		    vine::Exp *lhs = static_cast<vine::Move *>(s)->lhs, *rhs = static_cast<vine::Move *>(s)->rhs;
		    if (lhs->exp_type == vine::TEMP && rhs->exp_type == vine::TEMP) {
			vine::Temp *t0 = static_cast<vine::Temp *>(lhs), *t1 = static_cast<vine::Temp *>(rhs);
			// debug("%.8x %s\n", (*iit)->getAddress(), s->tostring().c_str());
			if (t1->name == "R_EBP") {
			    tempebp = t0->name;
			    // debug("EBP copied into %s\n", tempebp.c_str());
			}
		    } else if (lhs->exp_type == vine::TEMP && rhs->exp_type == vine::BINOP) {
			vine::BinOp *op = static_cast<vine::BinOp *>(rhs);
			if (op->binop_type == vine::PLUS && op->lhs->exp_type == vine::TEMP && 
			    op->rhs->exp_type == vine::CONSTANT) {
			    vine::Temp *base = static_cast<vine::Temp *>(op->lhs);
			    vine::Constant *offset = static_cast<vine::Constant *>(op->rhs);
			    if (base->name == tempebp) {
				// debug("%.8x %s\n", (*iit)->getAddress(), s->tostring().c_str());
				if (offset->val > 0 && offset->val < 0xFFFF) {
				    // debug("EBP is being used in an addition %.8x\n", (unsigned int) (offset->val & 0xFFFF));
                    argumentsno = utils::max(argumentsno, ((int)
                                (offset->val & 0xFFFF)) / 4);
				}
			    }
			}
		    }
		}
	    }
	}
    }

    if (argumentsno == -1)
	argumentsno = 0;
    else
	argumentsno--;
}

int Function::getArgumentsNo() {
    if (argumentsno == -1) {
	guessArgumentsNo();
    }

    return argumentsno;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
