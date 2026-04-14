# ─── SNTE Makefile (local build with Emscripten) ───
# Prerequisites: Install Emscripten SDK
#   https://emscripten.org/docs/getting_started/downloads.html

CC       = emcc
SRCS     = src/snte.c src/bindings.c
OUT      = docs/snte.js
CFLAGS   = -O2
LDFLAGS  = -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
           -s EXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
           -s ALLOW_MEMORY_GROWTH=1 \
           -s MODULARIZE=0

.PHONY: build clean serve

build:
	$(CC) $(SRCS) -o $(OUT) $(CFLAGS) $(LDFLAGS)
	@echo "✓ Built $(OUT) and docs/snte.wasm"

clean:
	rm -f docs/snte.js docs/snte.wasm

# Quick local preview (requires Python 3)
serve: build
	@echo "→ Serving at http://localhost:8080"
	cd docs && python3 -m http.server 8080
