/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/stack-trace.h>

#include <mgba/core/core.h>

#define CHECK_LENGTH() \
	if (written >= *length) { \
		*length = written; \
		return; \
	}

DEFINE_VECTOR(mStackFrames, struct mStackFrame);

void mStackTraceInit(struct mStackTrace* stack, size_t registersSize) {
	mStackFramesInit(&stack->stack, 0);
	stack->registersSize = registersSize;
}

void mStackTraceDeinit(struct mStackTrace* stack) {
	mStackTraceClear(stack);
	mStackFramesDeinit(&stack->stack);
}

void mStackTraceClear(struct mStackTrace* stack) {
	ssize_t i = mStackTraceGetDepth(stack) - 1;
	while (i >= 0) {
		free(mStackTraceGetFrame(stack, i)->regs);
		--i;
	}
	mStackFramesClear(&stack->stack);
}

size_t mStackTraceGetDepth(struct mStackTrace* stack) {
	return mStackFramesSize(&stack->stack);
}

struct mStackFrame* mStackTracePush(struct mStackTrace* stack, uint32_t pc, uint32_t destAddress, uint32_t sp, void* regs) {
	struct mStackFrame* frame = mStackFramesAppend(&stack->stack);
	frame->callAddress = pc;
	frame->entryAddress = destAddress;
	frame->frameBaseAddress = sp;
	frame->regs = malloc(stack->registersSize);
	frame->finished = false;
	frame->breakWhenFinished = false;
	frame->interrupt = false;
	memcpy(frame->regs, regs, stack->registersSize);
	return frame;
}

struct mStackFrame* mStackTraceGetFrame(struct mStackTrace* stack, uint32_t frame) {
	size_t depth = mStackTraceGetDepth(stack);
	if (frame >= depth) {
		return NULL;
	}
	return mStackFramesGetPointer(&stack->stack, depth - frame - 1);
}

void mStackTraceFormatFrame(struct mStackTrace* stack, uint32_t frame, char* out, size_t* length) {
	struct mStackFrame* stackFrame = mStackTraceGetFrame(stack, frame);
	struct mStackFrame* prevFrame = mStackTraceGetFrame(stack, frame + 1);
	size_t written = snprintf(out, *length, "#%d  ", frame);
	CHECK_LENGTH();
	if (prevFrame) {
		written += snprintf(out + written, *length - written, "%08X ", prevFrame->entryAddress);
		CHECK_LENGTH();
	}
	if (!stackFrame) {
		written += snprintf(out + written, *length - written, "no stack frame available)\n");
		*length = written;
		return;
	} else if (stack->formatRegisters) {
		written += snprintf(out + written, *length - written, "(");
		CHECK_LENGTH();
		char buffer[1024];
		size_t formattedSize = sizeof(buffer) - 2;
		stack->formatRegisters(stackFrame, buffer, &formattedSize);
		written += snprintf(out + written, *length - written, "%s)\n    ", buffer);
		CHECK_LENGTH();
	}
	if (prevFrame) {
		int32_t offset = stackFrame->callAddress - prevFrame->entryAddress;
		written += snprintf(out + written, *length - written, "at %08X [%08X+%d]\n", stackFrame->callAddress, prevFrame->entryAddress, offset);
	} else {
		written += snprintf(out + written, *length - written, "at %08X\n", stackFrame->callAddress);
	}
	*length = written;
}

void mStackTracePop(struct mStackTrace* stack) {
	size_t depth = mStackTraceGetDepth(stack);
	if (depth > 0) {
		struct mStackFrame* frame = mStackFramesGetPointer(&stack->stack, depth - 1);
		free(frame->regs);
		mStackFramesResize(&stack->stack, -1);
	}
}
