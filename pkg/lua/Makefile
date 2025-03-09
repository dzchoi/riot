PKG_NAME=lua
PKG_URL=https://github.com/lua/lua.git
# tag: v5.3.6
PKG_VERSION=75ea9ccbea7c4886f30da147fb67b693b2624c26
PKG_LICENSE=MIT

include $(RIOTBASE)/pkg/pkg.mk

CFLAGS += -Wno-cast-align

all: Makefile.lua
	$(QQ)"$(MAKE)" -C $(PKG_SOURCE_DIR) -f $(CURDIR)/Makefile.lua
