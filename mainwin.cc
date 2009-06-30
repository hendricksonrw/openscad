/*
 *  OpenSCAD (www.openscad.at)
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define INCLUDE_ABSTRACT_NODE_DETAILS

#include "openscad.h"

#include <QMenu>
#include <QMenuBar>
#include <QSplitter>
#include <QFileDialog>
#include <QApplication>
#include <QProgressDialog>

QPointer<MainWindow> current_win;

MainWindow::MainWindow(const char *filename)
{
        root_ctx.functions_p = &builtin_functions;
        root_ctx.modules_p = &builtin_modules;
	root_ctx.set_variable("$fs", Value(1.0));
	root_ctx.set_variable("$fa", Value(12.0));

	root_module = NULL;
	root_node = NULL;
	root_raw_term = NULL;
	root_norm_term = NULL;
	root_chain = NULL;
#ifdef ENABLE_CGAL
	root_N = NULL;
#endif

	{
		QMenu *menu = menuBar()->addMenu("&File");
		menu->addAction("&New", this, SLOT(actionNew()));
		menu->addAction("&Open...", this, SLOT(actionOpen()));
		menu->addAction("&Save", this, SLOT(actionSave()), QKeySequence(Qt::Key_F2));
		menu->addAction("Save &As...", this, SLOT(actionSaveAs()));
		menu->addAction("&Reload", this, SLOT(actionReload()), QKeySequence(Qt::Key_F3));
		menu->addAction("&Quit", this, SLOT(close()));
	}

	{
		QMenu *menu = menuBar()->addMenu("&Design");
		menu->addAction("&Reload and Compile", this, SLOT(actionReloadCompile()), QKeySequence(Qt::Key_F4));
		menu->addAction("&Compile", this, SLOT(actionCompile()), QKeySequence(Qt::Key_F5));
#ifdef ENABLE_CGAL
		menu->addAction("Compile and &Render (CGAL)", this, SLOT(actionRenderCGAL()), QKeySequence(Qt::Key_F6));
#endif
		menu->addAction("Display &AST...", this, SLOT(actionDisplayAST()));
		menu->addAction("Display CSG &Tree...", this, SLOT(actionDisplayCSGTree()));
		menu->addAction("Display CSG &Products...", this, SLOT(actionDisplayCSGProducts()));
		menu->addAction("Export as &STL...", this, SLOT(actionExportSTL()));
		menu->addAction("Export as &OFF...", this, SLOT(actionExportOFF()));
	}

	{
		QMenu *menu = menuBar()->addMenu("&View");
#ifdef ENABLE_OPENCSG
		actViewModeOpenCSG = menu->addAction("OpenCSG", this, SLOT(viewModeOpenCSG()));
		actViewModeOpenCSG->setCheckable(true);
#endif
#ifdef ENABLE_CGAL
		actViewModeCGALSurface = menu->addAction("CGAL Surfaces", this, SLOT(viewModeCGALSurface()));
		actViewModeCGALGrid = menu->addAction("CGAL Grid Only", this, SLOT(viewModeCGALGrid()));
		actViewModeCGALSurface->setCheckable(true);
		actViewModeCGALGrid->setCheckable(true);
#endif
		actViewModeThrownTogether = menu->addAction("Thrown Together", this, SLOT(viewModeThrownTogether()));
		actViewModeThrownTogether->setCheckable(true);

		menu->addSeparator();

		actViewModeWireframe = menu->addAction("Wireframe", this, SLOT(viewModeWireframe()));
		actViewModeWireframe->setCheckable(true);
		actViewModeShaded = menu->addAction("Shaded", this, SLOT(viewModeShaded()));
		actViewModeShaded->setCheckable(true);

		menu->addSeparator();
		menu->addAction("Top");
		menu->addAction("Bottom");
		menu->addAction("Left");
		menu->addAction("Right");
		menu->addAction("Front");
		menu->addAction("Back");
		menu->addAction("Diagonal");
		menu->addSeparator();
		menu->addAction("Perspective");
		menu->addAction("Orthogonal");
	}

	s1 = new QSplitter(Qt::Horizontal, this);
	editor = new QTextEdit(s1);
	s2 = new QSplitter(Qt::Vertical, s1);
	screen = new GLView(s2);
	console = new QTextEdit(s2);

	console->setReadOnly(true);
	current_win = this;

	PRINT("OpenSCAD (www.openscad.at)");
	PRINT("Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>");
	PRINT("");
	PRINT("This program is free software; you can redistribute it and/or modify");
	PRINT("it under the terms of the GNU General Public License as published by");
	PRINT("the Free Software Foundation; either version 2 of the License, or");
	PRINT("(at your option) any later version.");
	PRINT("");

	editor->setTabStopWidth(30);

	if (filename) {
		this->filename = QString(filename);
		setWindowTitle(this->filename);
		load();
	} else {
		setWindowTitle("New Document");
	}

#ifdef ENABLE_OPENCSG
	viewModeOpenCSG();
#else
	viewModeThrownTogether();
#endif
	viewModeShaded();

	setCentralWidget(s1);
	current_win = NULL;
}

MainWindow::~MainWindow()
{
	if (root_module)
		delete root_module;
	if (root_node)
		delete root_node;
#ifdef ENABLE_CGAL
	if (root_N)
		delete root_N;
#endif
}

void MainWindow::load()
{
	if (!filename.isEmpty())
	{
		QString text;
		FILE *fp = fopen(filename.toAscii().data(), "rt");
		if (!fp) {
			PRINTA("Failed to open file: %1 (%2)", filename, QString(strerror(errno)));
		} else {
			char buffer[513];
			int rc;
			while ((rc = fread(buffer, 1, 512, fp)) > 0) {
				buffer[rc] = 0;
				text += buffer;
			}
			fclose(fp);
			PRINTA("Loaded design `%1'.", filename);
		}
		editor->setPlainText(text);
	}
}

void MainWindow::compile()
{
	PRINT("Parsing design (AST generation)...");
	QApplication::processEvents();

	if (root_module) {
		delete root_module;
		root_module = NULL;
	}

	if (root_node) {
		delete root_node;
		root_node = NULL;
	}

	if (root_raw_term) {
		root_raw_term->unlink();
		root_raw_term = NULL;
	}

	if (root_norm_term) {
		root_norm_term->unlink();
		root_norm_term = NULL;
	}

	if (root_chain) {
		delete root_chain;
		root_chain = NULL;
	}

	root_module = parse(editor->toPlainText().toAscii().data(), false);

	if (!root_module)
		goto fail;

	PRINT("Compiling design (CSG Tree generation)...");
	QApplication::processEvents();

	AbstractNode::idx_counter = 1;
	root_node = root_module->evaluate(&root_ctx, QVector<QString>(), QVector<Value>(), QVector<ModuleInstanciation*>(), NULL);

	if (!root_node)
		goto fail;

	PRINT("Compiling design (CSG Products generation)...");
	QApplication::processEvents();

	double m[16];

	for (int i = 0; i < 16; i++)
		m[i] = i % 5 == 0 ? 1.0 : 0.0;

	root_raw_term = root_node->render_csg_term(m);

	if (!root_raw_term)
		goto fail;

	PRINT("Compiling design (CSG Products normalization)...");
	QApplication::processEvents();

	root_norm_term = root_raw_term->link();

	while (1) {
		CSGTerm *n = root_norm_term->normalize();
		root_norm_term->unlink();
		if (root_norm_term == n)
			break;
		root_norm_term = n;
	}

	if (!root_norm_term)
		goto fail;

	root_chain = new CSGChain();
	root_chain->import(root_norm_term);

	if (1) {
		PRINT("Compilation finished.");
	} else {
fail:
		PRINT("ERROR: Compilation failed!");
	}
}

void MainWindow::actionNew()
{
	filename = QString();
	setWindowTitle("New Document");
	editor->setPlainText("");
}

void MainWindow::actionOpen()
{
	current_win = this;
	QString new_filename = QFileDialog::getOpenFileName(this, "Open File", "", "OpenSCAD Designs (*.scad)");
	if (!new_filename.isEmpty())
	{
		filename = new_filename;
		setWindowTitle(filename);

		QString text;
		FILE *fp = fopen(filename.toAscii().data(), "rt");
		if (!fp) {
			PRINTA("Failed to open file: %1 (%2)", QString(filename), QString(strerror(errno)));
		} else {
			char buffer[513];
			int rc;
			while ((rc = fread(buffer, 1, 512, fp)) > 0) {
				buffer[rc] = 0;
				text += buffer;
			}
			fclose(fp);
			PRINTA("Loaded design `%1'.", QString(filename));
		}
		editor->setPlainText(text);
	}
	current_win = NULL;
}

void MainWindow::actionSave()
{
	current_win = this;
	FILE *fp = fopen(filename.toAscii().data(), "wt");
	if (!fp) {
		PRINTA("Failed to open file for writing: %1 (%2)", QString(filename), QString(strerror(errno)));
	} else {
		fprintf(fp, "%s", editor->toPlainText().toAscii().data());
		fclose(fp);
		PRINTA("Saved design `%1'.", QString(filename));
	}
	current_win = NULL;
}

void MainWindow::actionSaveAs()
{
	QString new_filename = QFileDialog::getSaveFileName(this, "Save File", filename, "OpenSCAD Designs (*.scad)");
	if (!new_filename.isEmpty()) {
		filename = new_filename;
		setWindowTitle(filename);
		actionSave();
	}
}

void MainWindow::actionReload()
{
	current_win = this;
	load();
	current_win = NULL;
}

void MainWindow::actionReloadCompile()
{
	current_win = this;
	console->clear();

	load();
	compile();

#ifdef ENABLE_OPENCSG
	if (!actViewModeOpenCSG->isChecked() && !actViewModeThrownTogether->isChecked()) {
		viewModeOpenCSG();
	}
	else
#endif
	{
		screen->updateGL();
	}
	current_win = NULL;
}

void MainWindow::actionCompile()
{
	current_win = this;
	console->clear();

	compile();

#ifdef ENABLE_OPENCSG
	if (!actViewModeOpenCSG->isChecked() && !actViewModeThrownTogether->isChecked()) {
		viewModeOpenCSG();
	}
	else
#endif
	{
		screen->updateGL();
	}
	current_win = NULL;
}

#ifdef ENABLE_CGAL

static void report_func(const class AbstractNode*, void *vp, int mark)
{
	QProgressDialog *pd = (QProgressDialog*)vp;
	int v = (int)((mark*100.0) / progress_report_count);
	pd->setValue(v < 100 ? v : 99);
	QApplication::processEvents();
}

void MainWindow::actionRenderCGAL()
{
	current_win = this;

	compile();

	if (!root_module || !root_node)
		return;

	if (root_N) {
		delete root_N;
		root_N = NULL;
	}

	PRINT("Rendering Polygon Mesh using CGAL...");
	QApplication::processEvents();

	QProgressDialog *pd = new QProgressDialog("Rendering Polygon Mesh using CGAL...", QString(), 0, 100);
	pd->setValue(0);
	pd->setAutoClose(false);
	pd->show();
	QApplication::processEvents();

	progress_report_prep(root_node, report_func, pd);
	root_N = new CGAL_Nef_polyhedron(root_node->render_cgal_nef_polyhedron());
	progress_report_fin();

	PRINTF("   Simple:     %6s", root_N->is_simple() ? "yes" : "no");
	PRINTF("   Valid:      %6s", root_N->is_valid() ? "yes" : "no");
	PRINTF("   Vertices:   %6d", (int)root_N->number_of_vertices());
	PRINTF("   Halfedges:  %6d", (int)root_N->number_of_halfedges());
	PRINTF("   Edges:      %6d", (int)root_N->number_of_edges());
	PRINTF("   Halffacets: %6d", (int)root_N->number_of_halffacets());
	PRINTF("   Facets:     %6d", (int)root_N->number_of_facets());
	PRINTF("   Volumes:    %6d", (int)root_N->number_of_volumes());

	if (!actViewModeCGALSurface->isChecked() && !actViewModeCGALGrid->isChecked()) {
		viewModeCGALSurface();
	} else {
		screen->updateGL();
	}

	PRINT("Rendering finished.");

	delete pd;
	current_win = NULL;

}

#endif /* ENABLE_CGAL */

void MainWindow::actionDisplayAST()
{
	current_win = this;
	QTextEdit *e = new QTextEdit(NULL);
	e->setTabStopWidth(30);
	e->setWindowTitle("AST Dump");
	if (root_module) {
		e->setPlainText(root_module->dump("", ""));
	} else {
		e->setPlainText("No AST to dump. Please try compiling first...");
	}
	e->show();
	e->resize(600, 400);
	current_win = NULL;
}

void MainWindow::actionDisplayCSGTree()
{
	current_win = this;
	QTextEdit *e = new QTextEdit(NULL);
	e->setTabStopWidth(30);
	e->setWindowTitle("CSG Tree Dump");
	if (root_node) {
		e->setPlainText(root_node->dump(""));
	} else {
		e->setPlainText("No CSG to dump. Please try compiling first...");
	}
	e->show();
	e->resize(600, 400);
	current_win = NULL;
}

void MainWindow::actionDisplayCSGProducts()
{
	current_win = this;
	QTextEdit *e = new QTextEdit(NULL);
	e->setTabStopWidth(30);
	e->setWindowTitle("CSG Products Dump");
	e->setPlainText(QString("\nCSG before normalization:\n%1\n\n\nCSG after normalization:\n%2\n\n\nCSG rendering chain:\n%3\n").arg(root_raw_term ? root_raw_term->dump() : "N/A", root_norm_term ? root_norm_term->dump() : "N/A", root_chain ? root_chain->dump() : "N/A"));
	e->show();
	e->resize(600, 400);
	current_win = NULL;
}

void MainWindow::actionExportSTL()
{
	current_win = this;
	PRINTA("Function %1 is not implemented yet!", QString(__PRETTY_FUNCTION__));
	current_win = NULL;
}

void MainWindow::actionExportOFF()
{
	current_win = this;
	PRINTA("Function %1 is not implemented yet!", QString(__PRETTY_FUNCTION__));
	current_win = NULL;
}

void MainWindow::viewModeActionsUncheck()
{
#ifdef ENABLE_OPENCSG
	actViewModeOpenCSG->setChecked(false);
#endif
#ifdef ENABLE_CGAL
	actViewModeCGALSurface->setChecked(false);
	actViewModeCGALGrid->setChecked(false);
#endif
	actViewModeThrownTogether->setChecked(false);
}

#ifdef ENABLE_OPENCSG

class OpenCSGPrim : public OpenCSG::Primitive
{
public:
	OpenCSGPrim(OpenCSG::Operation operation, unsigned int convexity) :
			OpenCSG::Primitive(operation, convexity) { }
	PolySet *p;
	virtual void render() {
		p->render_surface(PolySet::COLOR_NONE);
	}
};

static void renderGLviaOpenCSG(void *vp)
{
	MainWindow *m = (MainWindow*)vp;
	static int glew_initialized = 0;
	if (!glew_initialized) {
		glew_initialized = 1;
		glewInit();
	}

	if (m->root_chain) {
		GLint *shaderinfo = m->screen->shaderinfo;
		if (m->screen->useLights || !shaderinfo[0])
			shaderinfo = NULL;
		std::vector<OpenCSG::Primitive*> primitives;
		int j = 0;
		for (int i = 0;; i++)
		{
			bool last = i == m->root_chain->polysets.size();

			if (last || m->root_chain->types[i] == CSGTerm::UNION)
			{
				OpenCSG::render(primitives, OpenCSG::Goldfeather, OpenCSG::NoDepthComplexitySampling);
				glDepthFunc(GL_EQUAL);
				if (shaderinfo)
					glUseProgram(shaderinfo[0]);
				for (; j < i; j++) {
					if (m->root_chain->types[j] == CSGTerm::DIFFERENCE) {
						m->root_chain->polysets[j]->render_surface(PolySet::COLOR_CUTOUT, shaderinfo);
					} else {
						m->root_chain->polysets[j]->render_surface(PolySet::COLOR_MATERIAL, shaderinfo);
					}
				}
				if (shaderinfo)
					glUseProgram(0);
				for (unsigned int k = 0; k < primitives.size(); k++) {
					delete primitives[k];
				}
				glDepthFunc(GL_LESS);

				primitives.clear();
			}

			if (last)
				break;

			OpenCSGPrim *prim = new OpenCSGPrim(m->root_chain->types[i] == CSGTerm::DIFFERENCE ?
					OpenCSG::Subtraction : OpenCSG::Intersection, 1);
			prim->p = m->root_chain->polysets[i];
			primitives.push_back(prim);
		}
	}
}

void MainWindow::viewModeOpenCSG()
{
	viewModeActionsUncheck();
	actViewModeOpenCSG->setChecked(true);
	screen->renderfunc = renderGLviaOpenCSG;
	screen->renderfunc_vp = this;
	screen->updateGL();
}

#endif /* ENABLE_OPENCSG */

#ifdef ENABLE_CGAL

// a little hackish: we need access to default-private members of
// CGAL::OGL::Nef3_Converter so we can implement our own draw function
// that does not scale the model. so we define 'class' to 'struct'
// for this header..
//
// theoretically there could be two problems:
// 1.) defining language keyword with the pre processor is illegal afair
// 2.) the compiler could use a different memory layout or name mangling for structs
//
// both does not seam to be the case with todays compilers...
//
#define class struct
#include <CGAL/Nef_3/OGL_helper.h>
#undef class

static void renderGLviaCGAL(void *vp)
{
	MainWindow *m = (MainWindow*)vp;
	if (m->root_N) {
		CGAL::OGL::Polyhedron P;
		CGAL::OGL::Nef3_Converter<CGAL_Nef_polyhedron>::convert_to_OGLPolyhedron(*m->root_N, &P);
		P.init();
		if (m->actViewModeCGALSurface->isChecked())
			P.set_style(CGAL::OGL::SNC_BOUNDARY);
		if (m->actViewModeCGALGrid->isChecked())
			P.set_style(CGAL::OGL::SNC_SKELETON);
		glDisable(GL_LIGHTING);
#if 0
		P.draw();
#else
		if (P.style == CGAL::OGL::SNC_BOUNDARY) {
		  glCallList(P.object_list_+2);
		}
		glCallList(P.object_list_+1);
		glCallList(P.object_list_);
		if (P.switches[CGAL::OGL::SNC_AXES]) {
			glCallList(P.object_list_+3);
		}
#endif
	}
}

void MainWindow::viewModeCGALSurface()
{
	viewModeActionsUncheck();
	actViewModeCGALSurface->setChecked(true);
	screen->renderfunc = renderGLviaCGAL;
	screen->renderfunc_vp = this;
	screen->updateGL();
}

void MainWindow::viewModeCGALGrid()
{
	viewModeActionsUncheck();
	actViewModeCGALGrid->setChecked(true);
	screen->renderfunc = renderGLviaCGAL;
	screen->renderfunc_vp = this;
	screen->updateGL();
}

#endif /* ENABLE_CGAL */

static void renderGLThrownTogether(void *vp)
{
	MainWindow *m = (MainWindow*)vp;
	if (m->root_chain) {
		glDepthFunc(GL_LEQUAL);
#if 0
		glUseProgram(m->screen->shaderinfo[0]);
		for (int i = 0; i < m->root_chain->polysets.size(); i++) {
			if (m->root_chain->types[i] == CSGTerm::DIFFERENCE) {
				m->root_chain->polysets[i]->render_surface(PolySet::COLOR_CUTOUT, m->screen->shaderinfo);
			} else {
				m->root_chain->polysets[i]->render_surface(PolySet::COLOR_MATERIAL, m->screen->shaderinfo);
			}
		}
		glUseProgram(0);
#else
		for (int i = 0; i < m->root_chain->polysets.size(); i++) {
			if (m->root_chain->types[i] == CSGTerm::DIFFERENCE) {
				m->root_chain->polysets[i]->render_surface(PolySet::COLOR_CUTOUT);
				if (!m->screen->useLights)
					m->root_chain->polysets[i]->render_edges(PolySet::COLOR_CUTOUT);
			} else {
				m->root_chain->polysets[i]->render_surface(PolySet::COLOR_MATERIAL);
				if (!m->screen->useLights)
					m->root_chain->polysets[i]->render_edges(PolySet::COLOR_MATERIAL);
			}
		}
#endif
	}
}

void MainWindow::viewModeThrownTogether()
{
	viewModeActionsUncheck();
	actViewModeThrownTogether->setChecked(true);
	screen->renderfunc = renderGLThrownTogether;
	screen->renderfunc_vp = this;
	screen->updateGL();
}

void MainWindow::viewModeWireframe()
{
	screen->useLights = false;
	actViewModeWireframe->setChecked(true);
	actViewModeShaded->setChecked(false);
	screen->updateGL();
}

void MainWindow::viewModeShaded()
{
	screen->useLights = true;
	actViewModeWireframe->setChecked(false);
	actViewModeShaded->setChecked(true);
	screen->updateGL();
}
