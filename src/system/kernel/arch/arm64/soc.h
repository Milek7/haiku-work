#ifndef ARCH_ARM_SOC_H
#define ARCH_ARM_SOC_H

class InterruptController;

#include <drivers/bus/FDT.h>
#include <private/kernel/int.h>
#include <private/kernel/timer.h>

// ------------------------------------------------------ InterruptController

class InterruptController {
public:
	virtual void EnableInterrupt(int irq) = 0;
	virtual void DisableInterrupt(int irq) = 0;

	virtual void HandleInterrupt() = 0;

	static InterruptController* Get() {
		return sInstance;
	}

protected:
	InterruptController()
	{
		if (sInstance) {
			panic("Multiple InterruptController objects created; that is currently unsupported!");
		}
		sInstance = this;
	}

	static InterruptController *sInstance;
};

#endif // ARCH_ARM_SOC_H
