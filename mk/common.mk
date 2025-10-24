# Common build utilities

# Verbose control
# Usage: make V=1 for verbose output
V ?= 0
ifeq ("$(V)","1")
    Q :=
    VECHO := @printf
else
    Q := @
    VECHO := @printf
endif

# Color output
PASS_COLOR := \e[32;01m
NO_COLOR := \e[0m

# Pretty-print messages
# Usage: $(call notice, message)
notice = printf "$(PASS_COLOR)$(strip $1)$(NO_COLOR)\n"
