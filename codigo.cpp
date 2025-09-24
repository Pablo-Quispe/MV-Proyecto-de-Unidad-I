#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <fstream>
using namespace std;

struct Point { int x, y; Point(int x = 0, int y = 0) : x(x), y(y) {} };
struct Color { float r, g, b; Color(float r = 0.0f, float g = 0.0f, float b = 0.0f) : r(r), g(g), b(b) {} };
struct Figure { int type; vector<Point> points; Color color; int thickness; };

vector<Figure> figures, undoStack;
Color currentColor(0.0f, 0.0f, 0.0f);
int currentThickness = 1, currentAlgorithm = 0;
bool drawing = false, showGrid = false, showAxes = false;
vector<Point> currentPoints;
Point mousePos;
const int gridSpacing = 20, windowWidth = 800, windowHeight = 600;

void drawPixel(int x, int y, Color color, int thickness) {
    glColor3f(color.r, color.g, color.b);
    glPointSize(thickness);
    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();
}

void drawLine(Point p1, Point p2, Color color, int thickness, bool useDDA = false) {
    if (useDDA) {
        int dx = p2.x - p1.x, dy = p2.y - p1.y;
        int steps = max(abs(dx), abs(dy));
        if (steps == 0) { drawPixel(p1.x, p1.y, color, thickness); return; }

        float x = p1.x, y = p1.y;
        for (int i = 0; i <= steps; i++) {
            drawPixel(round(x), round(y), color, thickness);
            x += (float)dx / steps;
            y += (float)dy / steps;
        }
    } else {
        int dx = p2.x - p1.x, dy = p2.y - p1.y;
        if (dx == 0) {
            for (int y = min(p1.y, p2.y); y <= max(p1.y, p2.y); y++)
                drawPixel(p1.x, y, color, thickness);
            return;
        }

        float m = (float)dy / dx, b = p1.y - m * p1.x;
        if (abs(m) < 1) {
            for (int x = min(p1.x, p2.x); x <= max(p1.x, p2.x); x++)
                drawPixel(x, round(m * x + b), color, thickness);
        } else {
            for (int y = min(p1.y, p2.y); y <= max(p1.y, p2.y); y++)
                drawPixel(round((y - b) / m), y, color, thickness);
        }
    }
}

void drawCircle(Point center, int radius, Color color, int thickness) {
    int x = 0, y = radius, d = 1 - radius;

    auto plot = [&](int x, int y) {
        for (int i = -1; i <= 1; i += 2)
            for (int j = -1; j <= 1; j += 2) {
                drawPixel(center.x + i*x, center.y + j*y, color, thickness);
                drawPixel(center.x + i*y, center.y + j*x, color, thickness);
            }
    };

    plot(x, y);
    while (y > x) {
        if (d < 0) d += 2*x + 3;
        else { d += 2*(x - y) + 5; y--; }
        x++;
        plot(x, y);
    }
}

void drawEllipse(Point center, int rx, int ry, Color color, int thickness) {
    if (rx == 0 || ry == 0) return;

    int x = 0, y = ry, rx2 = rx*rx, ry2 = ry*ry;
    int d1 = ry2 - rx2*ry + rx2/4;

    auto plot = [&](int x, int y) {
        for (int i = -1; i <= 1; i += 2)
            for (int j = -1; j <= 1; j += 2)
                drawPixel(center.x + i*x, center.y + j*y, color, thickness);
    };

    while (2*ry2*x < 2*rx2*y) {
        plot(x, y);
        if (d1 < 0) d1 += ry2*(2*x + 3);
        else { d1 += ry2*(2*x + 3) + 2*rx2*(1 - y); y--; }
        x++;
    }

    int d2 = ry2*(x + 0.5)*(x + 0.5) + rx2*(y - 1)*(y - 1) - rx2*ry2;
    while (y >= 0) {
        plot(x, y);
        if (d2 > 0) d2 += rx2*(3 - 2*y);
        else { d2 += 2*ry2*(1 + x) + rx2*(3 - 2*y); x++; }
        y--;
    }
}

void drawGrid() {
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_LINES);
    for (int i = 0; i <= windowWidth; i += gridSpacing) {
        glVertex2i(i, 0); glVertex2i(i, windowHeight);
    }
    for (int i = 0; i <= windowHeight; i += gridSpacing) {
        glVertex2i(0, i); glVertex2i(windowWidth, i);
    }
    glEnd();
}

void drawAxes() {
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    glVertex2i(0, windowHeight/2); glVertex2i(windowWidth, windowHeight/2);
    glVertex2i(windowWidth/2, 0); glVertex2i(windowWidth/2, windowHeight);
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (showGrid) drawGrid();
    if (showAxes) drawAxes();

    for (const auto& f : figures) {
        if (f.points.size() < 2) continue;
        switch (f.type) {
            case 0: drawLine(f.points[0], f.points[1], f.color, f.thickness, false); break;
            case 1: drawLine(f.points[0], f.points[1], f.color, f.thickness, true); break;
            case 2: {
                int r = sqrt(pow(f.points[1].x - f.points[0].x, 2) + pow(f.points[1].y - f.points[0].y, 2));
                drawCircle(f.points[0], r, f.color, f.thickness);
                break;
            }
            case 3: {
                int rx = abs(f.points[1].x - f.points[0].x);
                int ry = f.points.size() > 2 ? abs(f.points[2].y - f.points[0].y) : rx/2;
                drawEllipse(f.points[0], rx, ry, f.color, f.thickness);
                break;
            }
        }
    }

    if (drawing && !currentPoints.empty()) {
        Color c = currentColor; int t = currentThickness;
        switch (currentAlgorithm) {
            case 0: case 1:
                drawLine(currentPoints[0], mousePos, c, t, currentAlgorithm == 1);
                break;
            case 2: {
                int r = sqrt(pow(mousePos.x - currentPoints[0].x, 2) + pow(mousePos.y - currentPoints[0].y, 2));
                drawCircle(currentPoints[0], r, c, t);
                break;
            }
            case 3: {
                int rx = abs(mousePos.x - currentPoints[0].x);
                int ry = currentPoints.size() > 1 ? abs(currentPoints[1].y - currentPoints[0].y) : abs(mousePos.y - currentPoints[0].y);
                drawEllipse(currentPoints[0], rx, ry, c, t);
                break;
            }
        }
    }

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    mousePos = Point(x, y);
    glutSetWindowTitle(("CAD 2D - (" + to_string(x) + ", " + to_string(y) + ")").c_str());

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (!drawing) { drawing = true; currentPoints.clear(); }
        currentPoints.push_back(Point(x, y));

        bool finished = (currentAlgorithm <= 1 && currentPoints.size() == 2) ||
                       (currentAlgorithm == 2 && currentPoints.size() == 2) ||
                       (currentAlgorithm == 3 && currentPoints.size() == 3);

        if (finished) {
            figures.push_back({currentAlgorithm, currentPoints, currentColor, currentThickness});
            undoStack.clear();
            drawing = false;
            currentPoints.clear();
        }
        glutPostRedisplay();
    }
}

void motion(int x, int y) {
    mousePos = Point(x, y);
    glutSetWindowTitle(("CAD 2D - (" + to_string(x) + ", " + to_string(y) + ")").c_str());
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    mousePos = Point(x, y);
    switch (key) {
        case 'g': case 'G': showGrid = !showGrid; break;
        case 'e': case 'E': showAxes = !showAxes; break;
        case 'c': case 'C': figures.clear(); undoStack.clear(); break;
        case 'z': case 'Z': if (!figures.empty()) { undoStack.push_back(figures.back()); figures.pop_back(); } break;
        case 'y': case 'Y': if (!undoStack.empty()) { figures.push_back(undoStack.back()); undoStack.pop_back(); } break;
    }
    glutPostRedisplay();
}

void menu(int option) {
    if (option < 4) currentAlgorithm = option;
    else if (option < 15) {
        Color colors[] = {{0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}, {1,0.5,0}};
        currentColor = colors[option-10];
    }
    else if (option < 24) {
        int thickness[] = {1,2,3,5};
        currentThickness = thickness[option-20];
    }
    else switch (option) {
        case 30: showGrid = !showGrid; break;
        case 31: showAxes = !showAxes; break;
        case 40: figures.clear(); undoStack.clear(); break;
        case 41: keyboard('z',0,0); break;
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("CAD 2D");

    glClearColor(1,1,1,1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, windowHeight, 0);

    int submenus[6];
    submenus[0] = glutCreateMenu(menu);
    glutAddMenuEntry("Recta (Directo)", 0); glutAddMenuEntry("Recta (DDA)", 1);
    glutAddMenuEntry("Circulo", 2); glutAddMenuEntry("Elipse", 3);

    submenus[1] = glutCreateMenu(menu);
    const char* colors[] = {"Negro","Rojo","Verde","Azul","Naranja"};
    for (int i = 0; i < 5; i++) glutAddMenuEntry(colors[i], 10+i);

    submenus[2] = glutCreateMenu(menu);
    const char* thickness[] = {"1 px","2 px","3 px","5 px"};
    for (int i = 0; i < 4; i++) glutAddMenuEntry(thickness[i], 20+i);

    int mainMenu = glutCreateMenu(menu);
    glutAddSubMenu("Dibujar", submenus[0]);
    glutAddSubMenu("Color", submenus[1]);
    glutAddSubMenu("Grosor", submenus[2]);
    glutAddMenuEntry("Cuadricula", 30);
    glutAddMenuEntry("Ejes", 31);
    glutAddMenuEntry("Limpiar", 40);
    glutAddMenuEntry("Deshacer", 41);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}
