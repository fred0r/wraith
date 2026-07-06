#ifndef _MODULE_H
#define _MODULE_H

namespace wraith {

class Module {
public:
	virtual ~Module() = default;
	virtual void init() = 0;
	virtual const char *name() const = 0;
};

} /* namespace wraith */

#endif /* !_MODULE_H */
