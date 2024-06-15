ifeq ("$(wildcard include/*)", "")
        LOCAL_DIRS="-Llib/ -Iinclude/"
endif

pursuit: pursuit.cpp
	g++ -Wall -Wextra -O2 $(LOCAL_DIRS) pursuit.cpp -lsfml-graphics -lsfml-window -lsfml-system -o pursuit

debug: pursuit.cpp
	g++ -Wall -Wextra -g -O0 $(LOCAL_DIRS) pursuit.cpp -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -o pursuit

static: pursuit.cpp
	g++ -Wall -Wextra -O2 $(LOCAL_DIRS) -DSFML_STATIC -static pursuit.cpp -lsfml-graphics-s -lsfml-window-s -lsfml-system-s -o pursuit

windows: pursuit.cpp
	x86_64-w64-mingw32-g++ -Wall -Wextra -O2 pursuit.cpp $(LOCAL_DIRS) -DSFML_STATIC -static \
	-lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -o pursuit
