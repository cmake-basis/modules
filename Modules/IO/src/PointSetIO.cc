/*
 * Medical Image Registration ToolKit (MIRTK)
 *
 * Copyright 2013-2016 Imperial College London
 * Copyright 2013-2016 Andreas Schuh
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mirtk/PointSetIO.h"

#include "mirtk/Path.h"
#include "mirtk/Stream.h"
#include "mirtk/System.h" // GetUserName, GetDateTime
#include "mirtk/Vtk.h"

#include "vtkPoints.h"
#include "vtkPointData.h"
#include "vtkDataArray.h"
#include "vtkCellArray.h"
#include "vtkIdTypeArray.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationIterator.h"

#include "vtkUnsignedShortArray.h"
#include "vtkFloatArray.h"

#include "vtkUnstructuredGrid.h"
#include "vtkStructuredGrid.h"

#include "vtkGenericDataObjectReader.h"
#include "vtkDataSetWriter.h"

#include "vtkPolyDataReader.h"
#include "vtkPolyDataWriter.h"

#include "vtkXMLGenericDataObjectReader.h"
#include "vtkXMLDataSetWriter.h"

#include "vtkXMLPolyDataReader.h"
#include "vtkXMLPolyDataWriter.h"

#include "vtkOBJReader.h"
#include "vtkPLYReader.h"
#include "vtkSTLReader.h"

#include "vtkPLYWriter.h"
#include "vtkSTLWriter.h"

#include "brainsuite/dfsurface.h"

#if MIRTK_IO_WITH_GIFTI
  #include "mirtk/NiftiImageInfo.h"
  #include "gifti/gifti_io.h"
#endif


namespace mirtk {


// =============================================================================
// File name extension
// =============================================================================

// -----------------------------------------------------------------------------
const char *DefaultExtension(vtkDataSet *dataset)
{
  if      (vtkPolyData        ::SafeDownCast(dataset)) return ".vtp";
  else if (vtkUnstructuredGrid::SafeDownCast(dataset)) return ".vtu";
  else if (vtkStructuredGrid  ::SafeDownCast(dataset)) return ".vts";
  return ".vtk";
}

// =============================================================================
// Generic I/O functions
// =============================================================================

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPointSet> ReadPointSet(const char *fname, int *ftype, bool exit_on_failure)
{
  vtkSmartPointer<vtkPointSet> pointset;
  const string ext = Extension(fname);
  if (ext == ".vtp" || ext == ".stl" || ext == ".ply" || ext == ".obj" || ext == ".dfs" || ext == ".off" || ext == ".gii") {
    pointset = ReadPolyData(fname);
  } else if (ext.length() == 4  && ext.substr(0, 3) == ".vt" && ext != ".vtk") {
    vtkSmartPointer<vtkXMLGenericDataObjectReader> reader;
    reader = vtkSmartPointer<vtkXMLGenericDataObjectReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    pointset = vtkPointSet::SafeDownCast(reader->GetOutput());
  } else {
    vtkSmartPointer<vtkGenericDataObjectReader> reader;
    reader = vtkSmartPointer<vtkGenericDataObjectReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    if (ftype) *ftype = reader->GetFileType();
    pointset = vtkPointSet::SafeDownCast(reader->GetOutput());
  }
  if (exit_on_failure && (!pointset || pointset->GetNumberOfPoints() == 0)) {
    cerr << "File " << fname << " either contains no points or could not be read" << endl;
    exit(1);
  }
  return pointset;
}

// -----------------------------------------------------------------------------
bool WritePointSet(const char *fname, vtkPointSet *pointset, bool compress, bool ascii)
{
  vtkPolyData *polydata = vtkPolyData::SafeDownCast(pointset);
  if (polydata) return WritePolyData(fname, polydata, compress, ascii);
  const string ext = Extension(fname);
  int success = 0;
  if (ext.length() == 4 && ext.substr(0, 3) == ".vt" && ext != ".vtk") {
    vtkSmartPointer<vtkXMLDataSetWriter> writer;
    writer = vtkSmartPointer<vtkXMLDataSetWriter>::New();
    SetVTKInput(writer, pointset);
    writer->SetFileName(fname);
    if (compress) writer->SetCompressorTypeToZLib();
    else          writer->SetCompressorTypeToNone();
    success = writer->Write();
  } else {
    vtkSmartPointer<vtkDataSetWriter> writer;
    writer = vtkSmartPointer<vtkDataSetWriter>::New();
    SetVTKInput(writer, pointset);
    writer->SetFileName(fname);
    if (ascii) writer->SetFileTypeToASCII();
    else       writer->SetFileTypeToBinary();
    success = writer->Write();
  }
  return (success == 1);
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> ReadPolyData(const char *fname, int *ftype, bool exit_on_failure)
{
  vtkSmartPointer<vtkPolyData> polydata;
  const string ext = Extension(fname);
  if (ext == ".vtp") {
    vtkSmartPointer<vtkXMLPolyDataReader> reader;
    reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    polydata = reader->GetOutput();
  } else if (ext == ".stl") {
    vtkSmartPointer<vtkSTLReader> reader;
    reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    polydata = reader->GetOutput();
  } else if (ext == ".ply") {
    vtkSmartPointer<vtkPLYReader> reader;
    reader = vtkSmartPointer<vtkPLYReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    polydata = reader->GetOutput();
  } else if (ext == ".obj") {
    vtkSmartPointer<vtkOBJReader> reader;
    reader = vtkSmartPointer<vtkOBJReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    polydata = reader->GetOutput();
  } else if (ext == ".dfs") {
    polydata = ReadDFS(fname);
  } else if (ext == ".off") {
    polydata = ReadOFF(fname);
  } else if (ext == ".gii") {
    #if MIRTK_IO_WITH_GIFTI
      polydata = ReadGIFTI(fname, nullptr, exit_on_failure);
    #else
      if (exit_on_failure) {
        cerr << "Error: File '" << fname << "' cannot be read because MIRTK I/O library was built without GIFTI support!" << endl;
        exit(1);
      }
    #endif
  } else {
    vtkSmartPointer<vtkPolyDataReader> reader;
    reader = vtkSmartPointer<vtkPolyDataReader>::New();
    reader->SetFileName(fname);
    reader->Update();
    if (ftype) *ftype = reader->GetFileType();
    polydata = reader->GetOutput();
  }
  if (exit_on_failure && polydata->GetNumberOfPoints() == 0) {
    cerr << "Error: File '" << fname << "' either contains no points or could not be read!" << endl;
    exit(1);
  }
  return polydata;
}

// -----------------------------------------------------------------------------
bool WritePolyData(const char *fname, vtkPolyData *polydata, bool compress, bool ascii)
{
  const string ext = Extension(fname);
  int success = 0;
  if (ext == ".vtp") {
    vtkSmartPointer<vtkXMLPolyDataWriter> writer;
    writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    SetVTKInput(writer, polydata);
    writer->SetFileName(fname);
    if (compress) writer->SetCompressorTypeToZLib();
    else          writer->SetCompressorTypeToNone();
    success = writer->Write();
  } else if (ext == ".stl") {
    vtkSmartPointer<vtkSTLWriter> writer;
    writer = vtkSmartPointer<vtkSTLWriter>::New();
    SetVTKInput(writer, polydata);
    if (ascii) writer->SetFileTypeToASCII();
    else       writer->SetFileTypeToBinary();
    writer->SetFileName(fname);
    success = writer->Write();
  } else if (ext == ".ply") {
    vtkSmartPointer<vtkPLYWriter> writer;
    writer = vtkSmartPointer<vtkPLYWriter>::New();
    SetVTKInput(writer, polydata);
    if (ascii) writer->SetFileTypeToASCII();
    else       writer->SetFileTypeToBinary();
    writer->SetFileName(fname);
    success = writer->Write();
  } else if (ext == ".node") {
    success = WriteTetGenNode(fname, polydata);
  } else if (ext == ".poly") {
    success = WriteTetGenPoly(fname, polydata);
  } else if (ext == ".smesh") {
    success = WriteTetGenSMesh(fname, polydata);
  } else if (ext == ".dfs") {
    success = WriteDFS(fname, polydata);
  } else if (ext == ".off") {
    success = WriteOFF(fname, polydata);
  } else if (ext == ".gii") {
    #if MIRTK_IO_WITH_GIFTI
      success = WriteGIFTI(fname, polydata, compress, ascii);
    #else
      cerr << "Error: Cannot write surface to GIFTI file because MIRTK I/O library was built without GIFTI support!" << endl;
    #endif
  } else {
    vtkSmartPointer<vtkPolyDataWriter> writer;
    writer = vtkSmartPointer<vtkPolyDataWriter>::New();
    SetVTKInput(writer, polydata);
    writer->SetFileName(fname);
    if (ascii) writer->SetFileTypeToASCII();
    else       writer->SetFileTypeToBinary();
    success = writer->Write();
  }
  return (success == 1);
}

// =============================================================================
// BrainSuite I/O functions
// =============================================================================

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> ReadDFS(const char *fname)
{
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  // Read .dfs file
  SILT::DFSurface surface;
  if (!surface.readDFS(fname)) return polydata;
  const vtkIdType npoints = static_cast<vtkIdType>(surface.vertices .size());
  const vtkIdType ncells  = static_cast<vtkIdType>(surface.triangles.size());
  // Copy vertex coordinates
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  points->SetNumberOfPoints(npoints);
  double p[3];
  for (vtkIdType i = 0; i < npoints; ++i) {
    p[0] = static_cast<double>(surface.vertices[i].x);
    p[1] = static_cast<double>(surface.vertices[i].y);
    p[2] = static_cast<double>(surface.vertices[i].z);
    points->SetPoint(i, p);
  }
  polydata->SetPoints(points);
  // Copy triangle face list
  vtkIdType pts[3];
  vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
  cells->Allocate(cells->EstimateSize(ncells, 3));
  for (vtkIdType i = 0; i < ncells; ++i) {
    pts[0] = static_cast<vtkIdType>(surface.triangles[i].a);
    pts[1] = static_cast<vtkIdType>(surface.triangles[i].b);
    pts[2] = static_cast<vtkIdType>(surface.triangles[i].c);
    cells->InsertNextCell(3, pts);
  }
  polydata->SetPolys(cells);
  // Copy vertex normals
  if (!surface.vertexNormals.empty()) {
    double n[3];
    vtkSmartPointer<vtkFloatArray> normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetName("Normals");
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(npoints);
    for (vtkIdType i = 0; i < npoints; ++i) {
      n[0] = static_cast<double>(surface.vertexNormals[i].x);
      n[1] = static_cast<double>(surface.vertexNormals[i].y);
      n[2] = static_cast<double>(surface.vertexNormals[i].z);
      normals->SetTuple(i, n);
    }
    polydata->GetPointData()->SetNormals(normals);
  }
  // Copy vertex colors
  if (!surface.vertexColors.empty()) {
    double rgb[3];
    vtkSmartPointer<vtkFloatArray> colors = vtkSmartPointer<vtkFloatArray>::New();
    colors->SetName("Colors");
    colors->SetNumberOfComponents(3);
    colors->SetNumberOfTuples(npoints);
    for (vtkIdType i = 0; i < npoints; ++i) {
      rgb[0] = static_cast<double>(surface.vertexColors[i].x);
      rgb[1] = static_cast<double>(surface.vertexColors[i].y);
      rgb[2] = static_cast<double>(surface.vertexColors[i].z);
      colors->SetTuple(i, rgb);
    }
    polydata->GetPointData()->AddArray(colors);
  }
  // Copy vertex UV coordinates
  if (!surface.vertexUV.empty()) {
    double uv[3] = {.0};
    vtkSmartPointer<vtkFloatArray> coords = vtkSmartPointer<vtkFloatArray>::New();
    coords->SetName("UV");
    coords->SetNumberOfComponents(3);
    coords->SetNumberOfTuples(npoints);
    for (vtkIdType i = 0; i < npoints; ++i) {
      uv[0] = static_cast<double>(surface.vertexUV[i].u);
      uv[1] = static_cast<double>(surface.vertexUV[i].v);
      coords->SetTuple(i, uv);
    }
    polydata->GetPointData()->SetTCoords(coords);
  }
  // Copy vertex labels
  if (!surface.vertexLabels.empty()) {
    vtkSmartPointer<vtkUnsignedShortArray> labels = vtkSmartPointer<vtkUnsignedShortArray>::New();
    labels->SetName("Labels");
    labels->SetNumberOfComponents(1);
    labels->SetNumberOfTuples(npoints);
    for (vtkIdType i = 0; i < npoints; ++i) {
      labels->SetValue(i, surface.vertexLabels[i]);
    }
    polydata->GetPointData()->AddArray(labels);
  }
  // Copy vertex attributes
  if (!surface.vertexAttributes.empty()) {
    vtkSmartPointer<vtkFloatArray> scalars = vtkSmartPointer<vtkFloatArray>::New();
    scalars->SetName("Attributes");
    scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfTuples(npoints);
    for (vtkIdType i = 0; i < npoints; ++i) {
      scalars->SetValue(i, surface.vertexAttributes[i]);
    }
    polydata->GetPointData()->SetScalars(scalars);
  }
  return polydata;
}

// -----------------------------------------------------------------------------
bool WriteDFS(const char *fname, vtkPolyData *polydata)
{
  SILT::DFSurface surface;
  // Copy vertex coordinates
  double p[3];
  surface.vertices.resize(polydata->GetNumberOfPoints());
  for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
    polydata->GetPoint(i, p);
    surface.vertices[i].x = static_cast<float>(p[0]);
    surface.vertices[i].y = static_cast<float>(p[1]);
    surface.vertices[i].z = static_cast<float>(p[2]);
  }
  // Copy triangular faces
  vtkIdType npts, *pts;
  surface.triangles.resize(polydata->GetNumberOfCells());
  for (vtkIdType i = 0; i < polydata->GetNumberOfCells(); ++i) {
    polydata->GetCellPoints(i, npts, pts);
    if (npts != 3) return false;
    surface.triangles[i].a = static_cast<int>(pts[0]);
    surface.triangles[i].b = static_cast<int>(pts[1]);
    surface.triangles[i].c = static_cast<int>(pts[2]);
  }
  // Copy vertex normals
  vtkDataArray *normals = polydata->GetPointData()->GetNormals();
  if (!normals) normals = polydata->GetPointData()->GetArray("Normals");
  if (normals) {
    double n[3];
    surface.vertexNormals.resize(polydata->GetNumberOfPoints());
    for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
      normals->GetTuple(i, n);
      surface.vertexNormals[i].x = static_cast<float>(n[0]);
      surface.vertexNormals[i].y = static_cast<float>(n[1]);
      surface.vertexNormals[i].z = static_cast<float>(n[2]);
    }
  }
  // Copy vertex colors
  vtkDataArray *colors = polydata->GetPointData()->GetArray("Colors");
  if (colors) {
    double rgb[3];
    surface.vertexColors.resize(polydata->GetNumberOfPoints());
    for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
      colors->GetTuple(i, rgb);
      surface.vertexColors[i].x = static_cast<float>(rgb[0]);
      surface.vertexColors[i].y = static_cast<float>(rgb[1]);
      surface.vertexColors[i].z = static_cast<float>(rgb[2]);
    }
  }
  // Copy vertex UV coordinates
  vtkDataArray *coords = polydata->GetPointData()->GetTCoords();
  if (!coords)  coords = polydata->GetPointData()->GetArray("UV");
  if (coords && (coords->GetNumberOfComponents() == 2 || coords->GetNumberOfComponents() == 3)) {
    surface.vertexUV.resize(polydata->GetNumberOfPoints());
    for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
      surface.vertexUV[i].u = static_cast<float>(coords->GetComponent(i, 0));
      surface.vertexUV[i].v = static_cast<float>(coords->GetComponent(i, 1));
    }
  }
  // Copy vertex labels
  vtkDataArray *labels = polydata->GetPointData()->GetArray("Labels");
  if (labels && labels->GetNumberOfComponents() == 1) {
    surface.vertexLabels.resize(polydata->GetNumberOfPoints());
    for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
      surface.vertexLabels[i] = static_cast<unsigned short>(labels->GetComponent(i, 0));
    }
  }
  // Copy vertex attributes
  vtkDataArray *scalars = polydata->GetPointData()->GetScalars();
  if (!scalars) scalars = polydata->GetPointData()->GetArray("Attributes");
  if (scalars && scalars->GetNumberOfComponents() == 1) {
    surface.vertexAttributes.resize(polydata->GetNumberOfPoints());
    for (vtkIdType i = 0; i < polydata->GetNumberOfPoints(); ++i) {
      surface.vertexAttributes[i] = static_cast<float>(scalars->GetComponent(i, 0));
    }
  }
  // Write .dfs file
  return surface.writeDFS(fname);
}

// =============================================================================
// Object File Format I/O functions
// =============================================================================

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> ReadOFF(const char *fname)
{
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();

  ifstream ifs(fname);
  if (!ifs) return polydata;

  string keyword;
  ifs >> keyword;
  if (keyword != "OFF" && keyword != "off") return polydata;

  int numVertices = -1, numFaces = -1, numEdges = -1;
  ifs >> numVertices >> numFaces >> numEdges;
  if (ifs.fail()) return polydata;

  if (numVertices < 0) numVertices = 0;
  if (numFaces    < 0) numFaces    = 0;

  vtkSmartPointer<vtkPoints> points;
  points = vtkSmartPointer<vtkPoints>::New();
  points->SetNumberOfPoints(numVertices);

  double p[3];
  for (int i = 0; i < numVertices; ++i) {
    ifs >> p[0] >> p[1] >> p[2];
    if (ifs.fail()) break;
    points->SetPoint(i, p);
  }
  if (ifs.fail()) return polydata;

  vtkSmartPointer<vtkCellArray> verts, lines, polys;
  vtkSmartPointer<vtkIdList> cell = vtkSmartPointer<vtkIdList>::New();
  verts = vtkSmartPointer<vtkCellArray>::New();
  lines = vtkSmartPointer<vtkCellArray>::New();
  polys = vtkSmartPointer<vtkCellArray>::New();
  for (int i = 0, ptId, n; i < numFaces; ++i) {
    ifs >> n;
    if (ifs.fail()) break;
    if (n > 0) {
      cell->Reset();
      for (int j = 0; j < n; ++j) {
        ifs >> ptId;
        cell->InsertNextId(ptId);
      }
      if      (n == 1) verts->InsertNextCell(cell);
      else if (n == 2) lines->InsertNextCell(cell);
      else             polys->InsertNextCell(cell);
      if (!ifs.good()) break;
    }
  }
  if (ifs.fail()) return polydata;

  verts->Squeeze();
  lines->Squeeze();
  polys->Squeeze();

  polydata->SetPoints(points);
  if (verts->GetNumberOfCells() > 0) polydata->SetVerts(verts);
  if (lines->GetNumberOfCells() > 0) polydata->SetLines(lines);
  if (polys->GetNumberOfCells() > 0) polydata->SetPolys(polys);

  return polydata;
}

// -----------------------------------------------------------------------------
bool WriteOFF(const char *fname, vtkPolyData *polydata)
{
  ofstream ofs(fname);
  ofs.precision(12);

  ofs << "OFF\n";
  ofs << polydata->GetNumberOfPoints() << " ";
  ofs << polydata->GetNumberOfCells()  << " 0\n";

  double p[3];
  for (vtkIdType ptId = 0; ptId < polydata->GetNumberOfPoints(); ++ptId) {
    polydata->GetPoint(ptId, p);
    ofs << p[0] << " " << p[1] << " " << p[2] << "\n";
  }

  vtkIdType numPts, *ptIds;
  polydata->BuildCells();
  for (vtkIdType cellId = 0; cellId < polydata->GetNumberOfCells(); ++cellId) {
    polydata->GetCellPoints(cellId, numPts, ptIds);
    ofs << numPts;
    for (vtkIdType i = 0; i < numPts; ++i) {
      ofs << " " << ptIds[i];
    }
    ofs << "\n";
  }

  return !ofs.fail();
}

// =============================================================================
// TetGen I/O functions
// =============================================================================

// -----------------------------------------------------------------------------
/// Write point set to TetGen .node output stream
///
/// @param[in, out] os       Output stream.
/// @param[in]      pointset Point set.
///
/// @return Whether point set was written successfully to the given output stream.
bool WriteTetGenNode(ostream &os, vtkPointSet *pointset)
{
  vtkPointData *pd = pointset->GetPointData();
  vtkDataArray *ar;
  int nattributes = 0;
  for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
    nattributes += pd->GetArray(i)->GetNumberOfComponents();
  }
  os << pointset->GetNumberOfPoints() << " 3 " << nattributes << " 0\n";
  double p[3];
  streamsize precision = os.precision();
  for (vtkIdType ptId = 0; ptId < pointset->GetNumberOfPoints(); ++ptId) {
    os << (ptId + 1) << " ";
    os.precision(8); // default TetGen tolerance is 1e-8
    pointset->GetPoint(ptId, p);
    os << " " << p[0] << " " << p[1] << " " << p[2];
    os.precision(5);
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      ar = pd->GetArray(i);
      for (int j = 0; j < ar->GetNumberOfComponents(); ++j) {
        os << " " << ar->GetComponent(ptId, j);
      }
    }
    os << "\n";
  }
  os.precision(precision);
  return !os.fail();
}

// -----------------------------------------------------------------------------
bool WriteTetGenNode(const char *fname, vtkPointSet *pointset)
{
  ofstream os(fname);
  if (!os.is_open()) return false;
  return WriteTetGenNode(os, pointset);
}

// -----------------------------------------------------------------------------
bool WriteTetGenPoly(const char *fname, vtkPolyData *polydata, const PointSet *holes)
{
  ofstream os(fname);
  if (!os.is_open()) return false;
  os << "# part 1: nodes\n";
  WriteTetGenNode(os, polydata);
  vtkIdType npts, *pts;
  vtkCellArray *verts  = polydata->GetVerts();
  vtkCellArray *lines  = polydata->GetLines();
  vtkCellArray *polys  = polydata->GetPolys();
  vtkCellArray *strips = polydata->GetStrips();
  int nfacets = 0;
  if (verts ->GetNumberOfCells() > 0) nfacets += 1;
  if (lines ->GetNumberOfCells() > 0) nfacets += 1;
  if (polys ->GetNumberOfCells() > 0) nfacets += 1;
  if (strips->GetNumberOfCells() > 0) nfacets += 1;
  os << "\n# part 2: facets\n";
  os << nfacets << " 0\n";
  if (verts->GetNumberOfCells() > 0) {
    os << "# verts\n";
    os << verts->GetNumberOfCells() << "\n";
    verts->InitTraversal();
    while (verts->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (lines->GetNumberOfCells() > 0) {
    os << "# lines\n";
    os << lines->GetNumberOfCells() << "\n";
    lines->InitTraversal();
    while (lines->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (polys->GetNumberOfCells() > 0) {
    os << "# polys\n";
    os << polys->GetNumberOfCells() << "\n";
    polys->InitTraversal();
    while (polys->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (strips->GetNumberOfCells() > 0) {
    os << "# strips\n";
    os << strips->GetNumberOfCells() << "\n";
    strips->InitTraversal();
    while (strips->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  os << "\n# part 3: hole list\n";
  if (holes) {
    os << holes->Size() << "\n";
    for (int i = 0; i < holes->Size(); ++i) {
      const Point &p = holes->GetPoint(i);
      os << (i+1) << " " << p._x << " " << p._y << " " << p._z << "\n";
    }
  } else {
    os << "0\n";
  }
  os << "\n# part 4: region list\n";
  os << "0\n";
  return !os.fail();
}

// -----------------------------------------------------------------------------
bool WriteTetGenSMesh(const char *fname, vtkPolyData *polydata, const PointSet *holes)
{
  ofstream os(fname);
  if (!os.is_open()) return false;
  os << "# part 1: nodes\n";
  WriteTetGenNode(os, polydata);
  vtkIdType npts, *pts;
  vtkCellArray *verts  = polydata->GetVerts();
  vtkCellArray *lines  = polydata->GetLines();
  vtkCellArray *polys  = polydata->GetPolys();
  vtkCellArray *strips = polydata->GetStrips();
  int nfacets = 0;
  if (verts ->GetNumberOfCells() > 0) nfacets += verts ->GetNumberOfCells();
  if (lines ->GetNumberOfCells() > 0) nfacets += lines ->GetNumberOfCells();
  if (polys ->GetNumberOfCells() > 0) nfacets += polys ->GetNumberOfCells();
  if (strips->GetNumberOfCells() > 0) nfacets += strips->GetNumberOfCells();
  os << "\n# part 2: facets\n";
  os << nfacets << " 0\n";
  if (verts->GetNumberOfCells() > 0) {
    verts->InitTraversal();
    while (verts->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (lines->GetNumberOfCells() > 0) {
    lines->InitTraversal();
    while (lines->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (polys->GetNumberOfCells() > 0) {
    polys->InitTraversal();
    while (polys->GetNextCell(npts, pts)) {
      os << npts << " ";
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  if (strips->GetNumberOfCells() > 0) {
    strips->InitTraversal();
    while (strips->GetNextCell(npts, pts)) {
      os << npts;
      for (vtkIdType i = 0; i < npts; ++i) os << " " << (pts[i] + 1);
      os << "\n";
    }
  }
  os << "\n# part 3: hole list\n";
  if (holes) {
    os << holes->Size() << "\n";
    for (int i = 0; i < holes->Size(); ++i) {
      const Point &p = holes->GetPoint(i);
      os << (i+1) << " " << p._x << " " << p._y << " " << p._z << "\n";
    }
  } else {
    os << "0\n";
  }
  os << "\n# part 4: region list\n";
  os << "0\n";
  return !os.fail();
}

// =============================================================================
// GIFTI I/O functions
// =============================================================================
#if MIRTK_IO_WITH_GIFTI

// -----------------------------------------------------------------------------
/// Get VTK data type enumeration value corresponding to given GIFTI datatype
static int GIFTIDataTypeToVTK(int datatype)
{
  switch (datatype) {
    case NIFTI_TYPE_INT8:    return VTK_CHAR;
    case NIFTI_TYPE_INT16:   return VTK_SHORT;
    case NIFTI_TYPE_INT32:   return VTK_INT;
    case NIFTI_TYPE_INT64:   return VTK_LONG_LONG;
    case NIFTI_TYPE_UINT8:   return VTK_UNSIGNED_CHAR;
    case NIFTI_TYPE_UINT16:  return VTK_UNSIGNED_SHORT;
    case NIFTI_TYPE_UINT32:  return VTK_UNSIGNED_INT;
    case NIFTI_TYPE_UINT64:  return VTK_UNSIGNED_LONG_LONG;
    case NIFTI_TYPE_FLOAT32: return VTK_FLOAT;
    case NIFTI_TYPE_FLOAT64: return VTK_DOUBLE;
    default:                 return VTK_VOID;
  }
}

// -----------------------------------------------------------------------------
/// Copy GIFTI data array of data type matching the template argument to vtkDataArray
template <class T>
void CopyGIFTIDataArrayWithDataType(vtkDataArray *dst, const giiDataArray *src,
                                    vtkIdTypeArray *indices = nullptr)
{
  const int m = src->dims[0];
  const int n = static_cast<int>(src->nvals / static_cast<long long>(m));
  const T *v = reinterpret_cast<const T *>(src->data);
  if (indices) {
    vtkIdType index;
    for (int i = 0; i < m; ++i)
    for (int j = 0; j < n; ++j) {
      dst->SetComponent(i, j, .0);
    }
    if (src->ind_ord == GIFTI_IND_ORD_COL_MAJOR) {
      for (int j = 0; j < n; ++j)
      for (int i = 0; i < m; ++i, ++v) {
        index = indices->GetComponent(i, 0);
        dst->SetComponent(index, j, static_cast<double>(*v));
      }
    } else {
      for (int i = 0; i < m; ++i) {
        index = indices->GetComponent(i, 0);
        for (int j = 0; j < n; ++j) {
          dst->SetComponent(index, j, static_cast<double>(*v));
        }
      }
    }
  } else {
    if (src->ind_ord == GIFTI_IND_ORD_COL_MAJOR) {
      for (int j = 0; j < n; ++j)
      for (int i = 0; i < m; ++i, ++v) {
        dst->SetComponent(i, j, static_cast<double>(*v));
      }
    } else {
      for (int i = 0; i < m; ++i)
      for (int j = 0; j < n; ++j) {
        dst->SetComponent(i, j, static_cast<double>(*v));
      }
    }
  }
}

// -----------------------------------------------------------------------------
/// Copy GIFTI data array to vtkDataArray
static void CopyGIFTIDataArray(vtkDataArray *dst, const giiDataArray *src,
                               vtkIdTypeArray *indices = nullptr)
{
  switch (src->datatype) {
    case NIFTI_TYPE_INT8:    CopyGIFTIDataArrayWithDataType<int8_t  >(dst, src, indices); break;
    case NIFTI_TYPE_INT16:   CopyGIFTIDataArrayWithDataType<int16_t >(dst, src, indices); break;
    case NIFTI_TYPE_INT32:   CopyGIFTIDataArrayWithDataType<int32_t >(dst, src, indices); break;
    case NIFTI_TYPE_INT64:   CopyGIFTIDataArrayWithDataType<int64_t >(dst, src, indices); break;
    case NIFTI_TYPE_UINT8:   CopyGIFTIDataArrayWithDataType<uint8_t >(dst, src, indices); break;
    case NIFTI_TYPE_UINT16:  CopyGIFTIDataArrayWithDataType<uint16_t>(dst, src, indices); break;
    case NIFTI_TYPE_UINT32:  CopyGIFTIDataArrayWithDataType<uint32_t>(dst, src, indices); break;
    case NIFTI_TYPE_UINT64:  CopyGIFTIDataArrayWithDataType<uint64_t>(dst, src, indices); break;
    case NIFTI_TYPE_FLOAT32: CopyGIFTIDataArrayWithDataType<float   >(dst, src, indices); break;
    case NIFTI_TYPE_FLOAT64: CopyGIFTIDataArrayWithDataType<double  >(dst, src, indices); break;
    default:
      cerr << "GIFTI data array has unknown/invalid data type: " << src->datatype << endl;
      exit(1);
  }
}

// -----------------------------------------------------------------------------
// vtkInformation keys of standard GIFTI meta data entries
#define GiftiMetaDataKeyMacro(getter, name, type) \
  vtkInformation##type##Key *GiftiMetaData::getter() \
  { \
    static vtkSmartPointer<vtkInformation##type##Key> key \
     = new vtkInformation##type##Key(name, "GiftiMetaData"); \
    return key; \
  }

GiftiMetaDataKeyMacro(DATE, "Date", String);
GiftiMetaDataKeyMacro(USER_NAME, "UserName", String);
GiftiMetaDataKeyMacro(SUBJECT_ID, "SubjectID", String);
GiftiMetaDataKeyMacro(SURFACE_ID, "SurfaceID", String);
GiftiMetaDataKeyMacro(UNIQUE_ID,  "UniqueID", String);
GiftiMetaDataKeyMacro(NAME, "Name", String);
GiftiMetaDataKeyMacro(DESCRIPTION, "Description", String);
GiftiMetaDataKeyMacro(TIME_STEP, "TimeStep", Double);
GiftiMetaDataKeyMacro(DATA_SPACE, "DataSpace", String);
GiftiMetaDataKeyMacro(ANATOMICAL_STRUCTURE_PRIMARY, "AnatomicalStructurePrimary", String);
GiftiMetaDataKeyMacro(ANATOMICAL_STRUCTURE_SECONDARY, "AnatomicalStructureSecondary", String);
GiftiMetaDataKeyMacro(GEOMETRIC_TYPE, "GeometricType", String);
GiftiMetaDataKeyMacro(TOPOLOGICAL_TYPE, "TopologicalType", String);
GiftiMetaDataKeyMacro(INTENT_CODE, "Intent_code", Integer);
GiftiMetaDataKeyMacro(INTENT_P1, "intent_p1", Double);
GiftiMetaDataKeyMacro(INTENT_P2, "intent_p2", Double);
GiftiMetaDataKeyMacro(INTENT_P3, "intent_p3", Double);

// -----------------------------------------------------------------------------
Array<vtkInformationKey *> GiftiMetaData::KeysForFile()
{
  Array<vtkInformationKey *> keys;
  keys.push_back(DATE());
  keys.push_back(USER_NAME());
  keys.push_back(DESCRIPTION());
  keys.push_back(SUBJECT_ID());
  keys.push_back(UNIQUE_ID());
  keys.push_back(TIME_STEP());
  return keys;
}

// -----------------------------------------------------------------------------
Array<vtkInformationKey *> GiftiMetaData::KeysForDataArray(int intent)
{
  Array<vtkInformationKey *> keys;
  keys.push_back(NAME());
  keys.push_back(DESCRIPTION());
  keys.push_back(UNIQUE_ID());
  keys.push_back(SUBJECT_ID());
  keys.push_back(SURFACE_ID());
  if (intent < 0) {
    keys.push_back(ANATOMICAL_STRUCTURE_PRIMARY());
    keys.push_back(ANATOMICAL_STRUCTURE_SECONDARY());
    keys.push_back(GEOMETRIC_TYPE());
    keys.push_back(TOPOLOGICAL_TYPE());
    keys.push_back(INTENT_CODE());
    keys.push_back(INTENT_P1());
    keys.push_back(INTENT_P2());
    keys.push_back(INTENT_P3());
  } else if (intent == NIFTI_INTENT_POINTSET) {
    keys.push_back(ANATOMICAL_STRUCTURE_PRIMARY());
    keys.push_back(ANATOMICAL_STRUCTURE_SECONDARY());
    keys.push_back(GEOMETRIC_TYPE());
  } else if (intent == NIFTI_INTENT_TRIANGLE) {
    keys.push_back(TOPOLOGICAL_TYPE());
  } else if (NIFTI_FIRST_STATCODE <= intent && intent <= NIFTI_LAST_STATCODE) {
    keys.push_back(INTENT_CODE());
    keys.push_back(INTENT_P1());
    keys.push_back(INTENT_P2());
    keys.push_back(INTENT_P3());
  }
  return keys;
}

// -----------------------------------------------------------------------------
/// Get GIFTI meta data from vtkInformation given a vtkInformationKey as string
string GiftiMetaData::Get(vtkInformation *info, vtkInformationKey *key)
{
  vtkInformationStringKey *skey = vtkInformationStringKey::SafeDownCast(key);
  if (skey) return info->Get(skey);
  vtkInformationDoubleKey *dkey = vtkInformationDoubleKey::SafeDownCast(key);
  if (dkey) return ToString(info->Get(dkey));
  vtkInformationIntegerKey *ikey = vtkInformationIntegerKey::SafeDownCast(key);
  if (dkey) return ToString(info->Get(ikey));
  return string();
}

// -----------------------------------------------------------------------------
/// Copy standard GIFTI meta data to vtkInformation
static void CopyGIFTIMetaData(vtkInformation *info, const giiMetaData &meta)
{
  for (int i = 0; i < meta.length; ++i) {
    if (strcmp(meta.name[i], GiftiMetaData::DATE()->GetName()) == 0) {
      info->Set(GiftiMetaData::DATE(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::USER_NAME()->GetName()) == 0) {
      info->Set(GiftiMetaData::USER_NAME(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::SUBJECT_ID()->GetName()) == 0) {
      info->Set(GiftiMetaData::SUBJECT_ID(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::SURFACE_ID()->GetName()) == 0) {
      info->Set(GiftiMetaData::SURFACE_ID(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::UNIQUE_ID()->GetName()) == 0) {
      info->Set(GiftiMetaData::UNIQUE_ID(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::NAME()->GetName()) == 0) {
      info->Set(GiftiMetaData::NAME(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::DESCRIPTION()->GetName()) == 0) {
      info->Set(GiftiMetaData::DESCRIPTION(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::TIME_STEP()->GetName()) == 0) {
      double tr;
      if (FromString(meta.value[i], tr)) {
        info->Set(GiftiMetaData::TIME_STEP(), tr);
      }
    } else if (strcmp(meta.name[i], GiftiMetaData::ANATOMICAL_STRUCTURE_PRIMARY()->GetName()) == 0) {
      info->Set(GiftiMetaData::ANATOMICAL_STRUCTURE_PRIMARY(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::ANATOMICAL_STRUCTURE_SECONDARY()->GetName()) == 0) {
      info->Set(GiftiMetaData::ANATOMICAL_STRUCTURE_SECONDARY(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::GEOMETRIC_TYPE()->GetName()) == 0) {
      info->Set(GiftiMetaData::GEOMETRIC_TYPE(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::TOPOLOGICAL_TYPE()->GetName()) == 0) {
      info->Set(GiftiMetaData::TOPOLOGICAL_TYPE(), meta.value[i]);
    } else if (strcmp(meta.name[i], GiftiMetaData::INTENT_CODE()->GetName()) == 0 ||
               strcmp(meta.name[i], "Intent")     == 0 ||
               strcmp(meta.name[i], "IntentCode") == 0) {
      int intent_code;
      if (FromString(meta.value[i], intent_code)) {
        info->Set(GiftiMetaData::INTENT_CODE(), intent_code);
      }
    } else if (strcmp(meta.name[i], GiftiMetaData::INTENT_P1()->GetName())  == 0 ||
               strcmp(meta.name[i], "Intent_p1") == 0 ||
               strcmp(meta.name[i], "IntentP1")  == 0) {
      double intent_p1;
      if (FromString(meta.value[i], intent_p1)) {
        info->Set(GiftiMetaData::INTENT_P1(), intent_p1);
      }
    } else if (strcmp(meta.name[i], GiftiMetaData::INTENT_P2()->GetName())  == 0 ||
               strcmp(meta.name[i], "Intent_p2") == 0 ||
               strcmp(meta.name[i], "IntentP2")  == 0) {
      double intent_p2;
      if (FromString(meta.value[i], intent_p2)) {
        info->Set(GiftiMetaData::INTENT_P2(), intent_p2);
      }
    } else if (strcmp(meta.name[i], GiftiMetaData::INTENT_P3()->GetName())  == 0 ||
               strcmp(meta.name[i], "Intent_p3") == 0 ||
               strcmp(meta.name[i], "IntentP3")  == 0) {
      double intent_p3;
      if (FromString(meta.value[i], intent_p3)) {
        info->Set(GiftiMetaData::INTENT_P3(), intent_p3);
      }
    }
  }
}

// -----------------------------------------------------------------------------
/// Copy standard GIFTI meta data from vtkInformation if present
#if 0 // unused
static void CopyGIFTIMetaData(giiMetaData &meta, vtkInformation *info)
{
  vtkNew<vtkInformationIterator> it;
  it->SetInformation(info);
  for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextItem()) {
    vtkInformationKey * const key = it->GetCurrentKey();
    if (info->Has(key)) {
      const string value = GiftiMetaData::Get(info, key);
      if (!value.empty()) {
        gifti_add_to_meta(&meta, key->GetName(), value.c_str(), 1);
      }
    }
  }
}
#endif

// -----------------------------------------------------------------------------
/// Copy specified standard GIFTI meta data from vtkInformation if present
static void CopyGIFTIMetaData(giiMetaData &meta, vtkInformation *info,
                              const Array<vtkInformationKey *> &keys)
{
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    vtkInformationKey * const key = *it;
    if (info->Has(key)) {
      const string value = GiftiMetaData::Get(info, key);
      if (!value.empty()) {
        gifti_add_to_meta(&meta, key->GetName(), value.c_str(), 1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
/// Copy GIFTI point set to vtkPoints
static vtkSmartPointer<vtkPoints>
GIFTICoordinatesToVTK(const gifti_image *gim, vtkInformation *info = nullptr, bool errmsg = false)
{
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  for (int i = 0; i < gim->numDA; ++i) {
    giiDataArray *array = gim->darray[i];
    if (array->intent == NIFTI_INTENT_POINTSET) {
      if (array->datatype != NIFTI_TYPE_FLOAT32) {
        if (errmsg) {
          cerr << "Error: GIFTI coordinates array must have datatype NIFTI_TYPE_FLOAT32!" << endl;
        }
        break;
      }
      if (array->num_dim != 2) {
        if (errmsg) {
          cerr << "Error: GIFTI coordinates array must have 2 dimensions!" << endl;
        }
        break;
      }
      if (array->dims[1] != 3) {
        if (errmsg) {
          cerr << "Error: Second dimension of GIFTI coordinates array must have size 3!" << endl;
        }
        break;
      }
      const int n = array->dims[0];
      points->SetNumberOfPoints(n);
      const float *x = reinterpret_cast<float *>(array->data);
      if (array->ind_ord == GIFTI_IND_ORD_COL_MAJOR) {
        const float *y = x + n, *z = y + n;
        for (int j = 0; j < n; ++j, ++x, ++y, ++z) {
          points->SetPoint(j, static_cast<double>(*x), static_cast<double>(*y), static_cast<double>(*z));
        }
      } else {
        const float *y = x + 1, *z = x + 2;
        for (int j = 0; j < n; ++j, x += 3, y += 3, z += 3) {
          points->SetPoint(j, static_cast<double>(*x), static_cast<double>(*y), static_cast<double>(*z));
        }
      }
      if (info) {
        CopyGIFTIMetaData(info, array->meta);
        const char *dataspace;
        if (array->numCS > 0) {
          dataspace = array->coordsys[0]->dataspace;
          for (int c = 1; c < array->numCS; ++c) {
            if (strcmp(dataspace, array->coordsys[c]->dataspace) != 0) {
              dataspace = "NIFTI_XFORM_UNKNOWN";
              break;
            }
          }
        } else {
          dataspace = "NIFTI_XFORM_UNKNOWN";
        }
        info->Set(GiftiMetaData::DATA_SPACE(), dataspace);
      }
      break;
    }
  }
  return points;
}

// -----------------------------------------------------------------------------
/// Copy GIFTI topology information to vtkCellArray
static vtkSmartPointer<vtkCellArray>
GIFTITopologyToVTK(const gifti_image *gim, vtkInformation *info = nullptr, bool errmsg = false)
{
  vtkSmartPointer<vtkCellArray> triangles;
  for (int i = 0; i < gim->numDA; ++i) {
    giiDataArray *array = gim->darray[i];
    if (array->intent == NIFTI_INTENT_TRIANGLE) {
      if (array->datatype != NIFTI_TYPE_INT32) {
        if (errmsg) {
          cerr << "Error: GIFTI topology array must have datatype NIFTI_TYPE_INT32!" << endl;
        }
        break;
      }
      if (array->num_dim != 2) {
        if (errmsg) {
          cerr << "Error: GIFTI topology array must have 2 dimensions!" << endl;
        }
        break;
      }
      if (array->dims[1] != 3) {
        if (errmsg) {
          cerr << "Error: Second dimension of GIFTI topology array must have size 3!" << endl;
        }
        break;
      }
      vtkIdType pts[3];
      const int n = array->dims[0];
      triangles = vtkSmartPointer<vtkCellArray>::New();
      triangles->Allocate(3 * n);
      const int *a = reinterpret_cast<int *>(array->data);
      if (array->ind_ord == GIFTI_IND_ORD_COL_MAJOR) {
        const int *b = a + n, *c = b + n;
        for (int j = 0; j < n; ++j, ++a, ++b, ++c) {
          pts[0] = static_cast<vtkIdType>(*a);
          pts[1] = static_cast<vtkIdType>(*b);
          pts[2] = static_cast<vtkIdType>(*c);
          triangles->InsertNextCell(3, pts);
        }
      } else {
        const int *b = a + 1, *c = a + 2;
        for (int j = 0; j < n; ++j, a += 3, b += 3, c += 3) {
          pts[0] = static_cast<vtkIdType>(*a);
          pts[1] = static_cast<vtkIdType>(*b);
          pts[2] = static_cast<vtkIdType>(*c);
          triangles->InsertNextCell(3, pts);
        }
      }
      if (info) CopyGIFTIMetaData(info, array->meta);
      break;
    }
  }
  return triangles;
}

// -----------------------------------------------------------------------------
/// Convert GIFTI node indices array to vtkDataArray
static vtkSmartPointer<vtkIdTypeArray>
GIFTINodeIndicesToVTK(const gifti_image *gim, bool errmsg = false)
{
  vtkSmartPointer<vtkIdTypeArray> indices;
  for (int i = 0; i < gim->numDA; ++i) {
    giiDataArray *array = gim->darray[i];
    if (array->intent == NIFTI_INTENT_NODE_INDEX) {
      if (array->num_dim != 1) {
        if (errmsg) {
          cerr << "Error: GIFTI node indices array must have 1 dimension!" << endl;
        }
        break;
      }
      if (array->dims[0] <= 0) {
        if (errmsg) {
          cerr << "Error: GIFTI node indices array must contain at least one index!" << endl;
        }
        break;
      }
      indices = vtkSmartPointer<vtkIdTypeArray>::New();
      indices->SetNumberOfComponents(1);
      indices->SetNumberOfTuples(array->dims[0]);
      CopyGIFTIDataArray(indices, array);
    }
  }
  return indices;
}

// -----------------------------------------------------------------------------
/// Convert GIFTI data arrays to vtkDataArray instances of a vtkPointData
static vtkSmartPointer<vtkPointData>
GIFTIPointDataToVTK(const gifti_image *gim, vtkIdType npoints = 0, vtkIdTypeArray *indices = nullptr, bool errmsg = false)
{
  vtkIdType nindices = 0;
  if (indices) {
    nindices = indices->GetNumberOfTuples();
    if (npoints == 0) {
      cerr << "GIFTIPointDataToVTK: Number of points cannot be zero when reading sparse point data arrays!" << endl;
      exit(1);
    }
    if (nindices > npoints) {
      cerr << "GIFTIPointDataToVTK: Number of points cannot be less then number of node indices!" << endl;
      exit(1);
    }
  }
  bool ok = true;
  vtkSmartPointer<vtkPointData> pd = vtkSmartPointer<vtkPointData>::New();
  for (int i = 0; i < gim->numDA; ++i) {
    giiDataArray *array = gim->darray[i];
    if (array->intent != NIFTI_INTENT_POINTSET &&
        array->intent != NIFTI_INTENT_TRIANGLE &&
        array->intent != NIFTI_INTENT_NODE_INDEX &&
        array->num_dim > 0 && array->dims[0] > 0 && array->nvals > 0) {
      const int ncomp = static_cast<int>(array->nvals / static_cast<long long>(array->dims[0]));
      vtkSmartPointer<vtkDataArray> data;
      data = NewVTKDataArray(GIFTIDataTypeToVTK(array->datatype));
      data->SetNumberOfComponents(ncomp);
      if (npoints) {
        if (( indices && static_cast<vtkIdType>(array->dims[0]) != nindices) ||
            (!indices && static_cast<vtkIdType>(array->dims[0]) != npoints)) {
          if (errmsg) {
            cerr << "Error: GIFTI array size does not match point set or node indices array size!" << endl;
          }
          ok = false;
          break;
        }
        data->SetNumberOfTuples(npoints);
      } else {
        data->SetNumberOfTuples(array->dims[0]);
      }
      CopyGIFTIDataArray(data, array, indices);
      vtkInformation * const info = data->GetInformation();
      CopyGIFTIMetaData(info, array->meta);
      if (info->Has(GiftiMetaData::NAME())) {
        data->SetName(info->Get(GiftiMetaData::NAME()));
      } else {
        data->SetName(ToString(array->intent).c_str());
      }
      const int idx = pd->AddArray(data);
      if (array->intent == NIFTI_INTENT_SHAPE && !pd->GetScalars()) {
        pd->SetActiveAttribute(idx, vtkDataSetAttributes::SCALARS);
      }
      if (array->intent == NIFTI_INTENT_VECTOR && ncomp == 3 && !pd->GetVectors()) {
        pd->SetActiveAttribute(idx, vtkDataSetAttributes::VECTORS);
      }
    }
  }
  if (!ok) pd->Initialize();
  return pd;
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPoints> ReadGIFTICoordinates(const char *fname, vtkInformation *info, bool errmsg)
{
  gifti_image *gim = gifti_read_image(fname, 1);
  if (gim == nullptr) {
    if (errmsg) {
      cerr << "Error: Could not read GIFTI file: " << fname << endl;
    }
    return vtkSmartPointer<vtkPoints>::New();
  }
  return GIFTICoordinatesToVTK(gim, info, errmsg);
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkCellArray> ReadGIFTITopology(const char *fname, vtkInformation *info, bool errmsg)
{
  gifti_image *gim = gifti_read_image(fname, 1);
  if (gim == nullptr) {
    if (errmsg) {
      cerr << "Error: Could not read GIFTI file: " << fname << endl;
    }
    return nullptr;
  }
  return GIFTITopologyToVTK(gim, info, errmsg);
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPointData> ReadGIFTIPointData(const char *fname, bool errmsg)
{
  gifti_image *gim = gifti_read_image(fname, 1);
  if (gim == nullptr) {
    if (errmsg) {
      cerr << "Error: Could not read GIFTI file: " << fname << endl;
    }
    return nullptr;
  }
  return GIFTIPointDataToVTK(gim, 0, nullptr, errmsg);
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> ReadGIFTI(const char *fname, vtkPolyData *surface, bool errmsg)
{
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();

  // Read GIFTI
  const int read_data = 1;
  gifti_image *gim = gifti_read_image(fname, read_data);
  if (gim == nullptr) return polydata;

  // Convert geometry and topology arrays including their meta data
  vtkSmartPointer<vtkInformation> geom_info = vtkSmartPointer<vtkInformation>::New();
  vtkSmartPointer<vtkInformation> topo_info = vtkSmartPointer<vtkInformation>::New();
  vtkSmartPointer<vtkPoints>    points = GIFTICoordinatesToVTK(gim, geom_info,  errmsg);
  vtkSmartPointer<vtkCellArray> polys  = GIFTITopologyToVTK   (gim, topo_info,  errmsg);

  // Polygonal dataset requires a point set
  if (points->GetNumberOfPoints() == 0) {
    if (surface && surface->GetNumberOfPoints() > 0) {
      points = surface->GetPoints();
    } else {
      if (errmsg) {
        cerr << "Error: Cannot read GIFTI point data without input point set (e.g., from .coords.gii or .surf.gii file)!" << endl;
      }
      return polydata;
    }
  }
  const vtkIdType npoints = points->GetNumberOfPoints();

  // Check topology information
  polys->InitTraversal();
  vtkIdType npts, *pts;
  while (polys->GetNextCell(npts, pts) != 0) {
    if (npts != 3 || pts[0] >= npoints || pts[1] >= npoints || pts[2] >= npoints) {
      if (errmsg) {
        cerr << "Error: GIFTI topology array has invalid point index!" << endl;
      }
      return polydata;
    }
  }

  // Get node indices array
  vtkSmartPointer<vtkIdTypeArray> indices = GIFTINodeIndicesToVTK(gim, errmsg);
  if (indices) {
    for (vtkIdType i = 0; i < indices->GetNumberOfTuples(); ++i) {
      const vtkIdType index = indices->GetComponent(i, 0);
      if (index >= npoints) {
        if (errmsg) {
          cerr << "Error: Index of GIFTI node indices array element is out of range!" << endl;
          cerr << "       - Number of points = " << npoints << endl;
          cerr << "       - Node index       = " << index << endl;
        }
        return polydata;
      }
    }
  }

  // Convert possibly sparse point data arrays
  vtkSmartPointer<vtkPointData> pd = GIFTIPointDataToVTK(gim, npoints, indices, errmsg);

  // Copy file meta data to vtkPolyData information
  vtkInformation * const info = polydata->GetInformation();
  CopyGIFTIMetaData(info, gim->meta);

  // Free gifti_image instance
  gifti_free_image(gim);
  gim = nullptr;

  // Check number of tuples of point data arrays
  bool ok = true;
  if (pd) {
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      vtkDataArray *array = pd->GetArray(i);
      if (array->GetNumberOfTuples() != npoints) {
        cerr << "Error: GIFTI array '" << array->GetName()
             << "' at index " << i << " has mismatching size!" << endl;
        cerr << "       - Number of points = " << npoints << endl;
        cerr << "       - Number of tuples = " << array->GetNumberOfTuples() << endl;
        ok = false;
      }
    }
  }

  // Finalize polygonal dataset
  if (ok) {
    // Set geometry, topology, and point data
    polydata->SetPoints(points);
    polydata->SetPolys(polys);
    polydata->GetPointData()->ShallowCopy(pd);
    // Copy meta data of geometry and topology data arrays to vtkPolyData information
    if (geom_info->Has(GiftiMetaData::SUBJECT_ID())) {
      info->CopyEntry(geom_info, GiftiMetaData::SUBJECT_ID());
    }
    if (geom_info->Has(GiftiMetaData::SURFACE_ID())) {
      info->CopyEntry(geom_info, GiftiMetaData::SURFACE_ID());
    }
    if (geom_info->Has(GiftiMetaData::UNIQUE_ID())) {
      info->CopyEntry(geom_info, GiftiMetaData::UNIQUE_ID());
    }
    if (geom_info->Has(GiftiMetaData::DESCRIPTION())) {
      info->CopyEntry(geom_info, GiftiMetaData::DESCRIPTION());
    }
    if (geom_info->Has(GiftiMetaData::DATA_SPACE())) {
      info->CopyEntry(geom_info, GiftiMetaData::DATA_SPACE());
    }
    if (geom_info->Has(GiftiMetaData::ANATOMICAL_STRUCTURE_PRIMARY())) {
      info->CopyEntry(geom_info, GiftiMetaData::ANATOMICAL_STRUCTURE_PRIMARY());
    }
    if (geom_info->Has(GiftiMetaData::ANATOMICAL_STRUCTURE_SECONDARY())) {
      info->CopyEntry(geom_info, GiftiMetaData::ANATOMICAL_STRUCTURE_SECONDARY());
    }
    if (geom_info->Has(GiftiMetaData::GEOMETRIC_TYPE())) {
      info->CopyEntry(geom_info, GiftiMetaData::GEOMETRIC_TYPE());
    }
    if (topo_info->Has(GiftiMetaData::TOPOLOGICAL_TYPE())) {
      info->CopyEntry(topo_info, GiftiMetaData::TOPOLOGICAL_TYPE());
    }
  } else {
    info->Clear();
  }

  return polydata;
}

// -----------------------------------------------------------------------------
bool AddGIFTICoordinates(gifti_image *gim, vtkPoints *points, vtkInformation *info = nullptr)
{
  if (gifti_add_empty_darray(gim, 1) != 0) return false;
  giiDataArray *da = gim->darray[gim->numDA-1];

  // Set data array attributes
  da->intent     = NIFTI_INTENT_POINTSET;
  da->datatype   = NIFTI_TYPE_FLOAT32;
  da->ind_ord    = GIFTI_IND_ORD_ROW_MAJOR;
  da->num_dim    = 2;
  da->dims[0]    = static_cast<int>(points->GetNumberOfPoints());
  da->dims[1]    = 3;
  #ifdef HAVE_ZLIB
    da->encoding = GIFTI_ENCODING_B64GZ;
  #else
    da->encoding = GIFTI_ENCODING_B64BIN;
  #endif
  da->endian     = gifti_get_this_endian();
  da->ext_fname  = nullptr;
  da->ext_offset = 0;
  da->nvals      = gifti_darray_nvals(da);
  gifti_datatype_sizes(da->datatype, &da->nbyper, nullptr);

  // Allocate memory for point set coordinates
  da->data = calloc(da->nvals * da->nbyper, sizeof(char));
  if (da->data == nullptr) {
    gifti_free_DataArray(da);
    gim->darray[--gim->numDA] = nullptr;
    return false;
  }

  // Copy point set coordinates
  double p[3];
  float *pdata = reinterpret_cast<float *>(da->data);
  for (int i = 0; i < da->dims[0]; ++i) {
    points->GetPoint(i, p);
    (*pdata) = static_cast<float>(p[0]), ++pdata;
    (*pdata) = static_cast<float>(p[1]), ++pdata;
    (*pdata) = static_cast<float>(p[2]), ++pdata;
  }

  // Add coordinate system with identity matrix
  if (gifti_add_empty_CS(da) != 0) {
    gifti_free_DataArray(da);
    gim->darray[--gim->numDA] = nullptr;
    return false;
  }
  giiCoordSystem *cs = da->coordsys[da->numCS-1];
  const char *dataspace;
  if (info && info->Has(GiftiMetaData::DATA_SPACE())) {
    dataspace = info->Get(GiftiMetaData::DATA_SPACE());
  } else {
    dataspace = "NIFTI_XFORM_UNKNOWN";
  }
  cs->dataspace  = gifti_strdup(dataspace);
  cs->xformspace = gifti_strdup(dataspace);
  cs->xform[0][0] = cs->xform[1][1] = cs->xform[2][2] = cs->xform[3][3] = 1.0;

  // Copy meta data from vtkPolyData information
  if (info) {
    CopyGIFTIMetaData(da->meta, info, GiftiMetaData::KeysForDataArray(da->intent));
  }

  return true;
}

// -----------------------------------------------------------------------------
bool AddGIFTITopology(gifti_image *gim, vtkCellArray *triangles, vtkInformation *info = nullptr)
{
  if (triangles->GetMaxCellSize() != 3) return false;

  if (gifti_add_empty_darray(gim, 1) != 0) return false;
  giiDataArray *da = gim->darray[gim->numDA-1];

  // Set data array attributes
  da->intent     = NIFTI_INTENT_TRIANGLE;
  da->datatype   = NIFTI_TYPE_INT32;
  da->ind_ord    = GIFTI_IND_ORD_ROW_MAJOR;
  da->num_dim    = 2;
  da->dims[0]    = static_cast<int>(triangles->GetNumberOfCells());
  da->dims[1]    = 3;
  #ifdef HAVE_ZLIB
    da->encoding = GIFTI_ENCODING_B64GZ;
  #else
    da->encoding = GIFTI_ENCODING_B64BIN;
  #endif
  da->endian     = gifti_get_this_endian();
  da->ext_fname  = nullptr;
  da->ext_offset = 0;
  da->nvals      = gifti_darray_nvals(da);
  gifti_datatype_sizes(da->datatype, &da->nbyper, nullptr);

  // Allocate memory for point indices
  da->data = calloc(da->nvals * da->nbyper, sizeof(char));
  if (da->data == nullptr) {
    gifti_free_DataArray(da);
    gim->darray[--gim->numDA] = nullptr;
    return false;
  }

  // Copy triangles
  vtkIdType npts, *pts;
  int *pdata = reinterpret_cast<int *>(da->data);
  triangles->InitTraversal();
  for (int i = 0; i < da->dims[0]; ++i) {
    triangles->GetNextCell(npts, pts);
    if (npts != 3) {
      gifti_free_DataArray(da);
      gim->darray[--gim->numDA] = nullptr;
      return false;
    }
    (*pdata) = static_cast<int>(pts[0]), ++pdata;
    (*pdata) = static_cast<int>(pts[1]), ++pdata;
    (*pdata) = static_cast<int>(pts[2]), ++pdata;
  }

  // Copy meta data from vtkPolyData information
  if (info) {
    CopyGIFTIMetaData(da->meta, info, GiftiMetaData::KeysForDataArray(da->intent));
  }

  return true;
}

// -----------------------------------------------------------------------------
bool WriteGIFTI(const char *fname, vtkPolyData *polydata, bool compress, bool ascii)
{
  // Determine type of GIFTI file from file name extensions
  const string ext  = Extension(fname, EXT_Last);

  string type;
  if (ext == ".gii") {
    const string name = fname;
    type = Extension(name.substr(0, name.length() - ext.length()), EXT_Last);
    if (type == ".label" || type == ".time") {
      cerr << "WriteGIFTI: Output file type " << type << ext << " not supported" << endl;
      return false;
    }
    if (type != ".coord" &&
        type != ".func" &&
        type != ".label" &&
        type != ".rgba" &&
        type != ".shape" &&
        type != ".surf" &&
        type != ".tensor" &&
        type != ".time" &&
        type != ".topo" &&
        type != ".vector") {
      type.clear();
    }
  }

  // Allocate new GIFTI structure
  gifti_image *gim = gifti_create_image(0, 0, 0, 0, nullptr, 0);
  if (gim == nullptr) return false;

  // Set extra attributes for XML validation
  gifti_add_to_nvpairs(&gim->ex_atrs, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  gifti_add_to_nvpairs(&gim->ex_atrs, "xsi:noNamespaceSchemaLocation", "http://brainvis.wustl.edu/caret6/xml_schemas/GIFTI_Caret.xsd");

  // Copy file level meta data from vtkPolyData information
  vtkInformation * const info = polydata->GetInformation();
  CopyGIFTIMetaData(gim->meta, info, GiftiMetaData::KeysForFile());

  // Set UserName and Date
  gifti_add_to_meta(&gim->meta, GiftiMetaData::DATE()     ->GetName(), GetDateTime().c_str(), 1);
  gifti_add_to_meta(&gim->meta, GiftiMetaData::USER_NAME()->GetName(), GetUserName().c_str(), 1);

  // Add point coordinates
  if (polydata->GetNumberOfPoints() > 0) {
    if (type.empty() || type == ".coord" || type == ".surf") {
      AddGIFTICoordinates(gim, polydata->GetPoints(), info);
    }
  }

  // Add triangles
  if (polydata->GetPolys() && polydata->GetPolys()->GetNumberOfCells() > 0) {
    if (type.empty() || type == ".topo" || type == ".surf") {
      AddGIFTITopology(gim, polydata->GetPolys(), info);
    }
  }

  // TODO: Add point data arrays
  if (type.empty() || (type != ".coord" && type != ".topo" && type != ".surf")) {
  }

  // Set encoding of all data arrays
  #ifndef HAVE_ZLIB
    compress = false;
  #endif
  int encoding = (ascii      ? GIFTI_ENCODING_ASCII :
                   (compress ? GIFTI_ENCODING_B64GZ :
                               GIFTI_ENCODING_B64BIN));
  for (int i = 0; i < gim->numDA; ++i) {
    gim->darray[i]->encoding = encoding;
  }

  // Write GIFTI file
  const int write_data = 1;
  bool success = (gifti_write_image(gim, fname, write_data) == 0);
  gifti_free_image(gim);

  return success;
}

#endif // MIRTK_IO_WITH_GIFTI


} // namespace mirtk
