WATCH_FILES=find . -type f -not -path '*/\.*' -and -not -path '*/build/*' | grep -i '.*[.]\(c\|c\|h\)$$' 2> /dev/null

all: build

mkdir_build:
	[ -d ./build ] | mkdir -p build

entr_warn:
	@echo "----------------------------------------------------------"
	@echo "     ! File watching functionality non-operational !      "
	@echo ""
	@echo "Install entr(1) to automatically run tasks on file change."
	@echo "See http://entrproject.org/"
	@echo "----------------------------------------------------------"

build: mkdir_build
	cd build; cmake ..
	make -C build
