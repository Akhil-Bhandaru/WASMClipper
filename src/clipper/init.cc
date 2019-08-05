#define DEBUG 0

#include <iostream>
#include <node.h>
#include <v8.h>
#include <nan.h>
#include "clipper.hpp"
#include "clipper.cpp"

using namespace node;
using namespace v8;
using namespace ClipperLib;
// keep this value in sync with JSClipper
const double doubleFactor = 1000000;
int32_t wdebug = 0;

// Helper function
v8::Local<v8::String> v8String(std::string s) {
  return Nan::New(s).ToLocalChecked();
}

Paths v8ArrayToPaths(v8::Local<v8::Array> inOutPaths, bool doubleType) {
  uint32_t len = inOutPaths->Length();
  Paths polyshape(len);
#ifdef DEBUG
  if (wdebug > 1) std::cout << "polygonArray: length: " << len << std::endl;
#endif

  //no polyshape with less than 1 polygon possible
  if (len < 1) return polyshape;
  // http://izs.me/v8-docs/classv8_1_1Value.html
  for (uint32_t i = 0; i < len; i++) {
    if (!inOutPaths->Get(i)->IsArray()) continue;
    v8::Local<v8::Array> polyLine = v8::Local<v8::Array>::Cast(inOutPaths->Get(i));
#ifdef DEBUG
    if (wdebug > 2) std::cout << "polyLine: length: " << polyLine->Length() << std::endl;
#endif

    //no polygon with less than 3 points
    if (polyLine->Length() < 3) continue;

    for (unsigned int j = 0; j < polyLine->Length(); j++) {
      v8::Local<v8::Object> point = polyLine->Get(j)->ToObject();

      v8::Local<v8::Value> x = point->Get(v8String("x"));
      v8::Local<v8::Value> y = point->Get(v8String("y"));
#ifdef DEBUG
      if (wdebug > 3) {
        std::cout << "polyLine: point: " << *Nan::Utf8String(x) << " ";
        std::cout << *Nan::Utf8String(y) << std::endl;
      }
#endif
      IntPoint p;
      if (doubleType) {
        p = IntPoint(
              x->NumberValue() * doubleFactor,
              y->NumberValue() * doubleFactor
            );
      } else {
        p = IntPoint(
              x->NumberValue(),
              y->NumberValue()
            );
      }
      polyshape[i].push_back(p);
#ifdef DEBUG
      if (wdebug > 3) std::cout << "polyLine: point: " << p.X << " " << p.Y << std::endl;
#endif
    }
  }
  return polyshape;
}

v8::Local<Array> pathsToV8Array(Paths polygons, bool doubleType) {
  v8::Local<v8::Array> result = Nan::New<v8::Array>();
  for (unsigned int i = 0; i < polygons.size(); i++) {
    //no polygons with less than 3 points
    if (polygons[i].size() < 3) continue;
    v8::Local<v8::Array> points = Nan::New<v8::Array>();
    for (unsigned int k = 0; k < polygons[i].size(); k++) {
      IntPoint ip = polygons[i][k];
      v8::Local<v8::Number> x;
      v8::Local<v8::Number> y;
      if (doubleType) {
        x = Nan::New((double)ip.X / (double)doubleFactor);
        y = Nan::New((double)ip.Y / (double)doubleFactor);
      } else {
        x = Nan::New<v8::Number>(ip.X);
        y = Nan::New<v8::Number>(ip.Y);
      }
#ifdef DEBUG
      if (wdebug > 3) {
        std::cout << "polyLine: point: " << *Nan::Utf8String(x) << " ";
        std::cout << *Nan::Utf8String(y) << std::endl;
      }
#endif
      v8::Local<Object> point = Nan::New<v8::Object>();
      point->Set(v8String("x"), x);
      point->Set(v8String("y"), y);
      points->Set(points->Length(), point);
    }
    result->Set(result->Length(), points);
  }
  return result;
}

void doFixOrientation(Paths &polyshape) {
  if (!ClipperLib::Orientation(polyshape[0])) {
    ClipperLib::ReversePath(polyshape[0]);
#ifdef DEBUG
    if (wdebug > 0) std::cout << "doFixOrientation: outerPoints reversed" << std::endl;
#endif
  }

  for (unsigned int i = 1; i < polyshape.size(); i++) {
    if (ClipperLib::Orientation(polyshape[i])) {
      ClipperLib::ReversePath(polyshape[i]);
#ifdef DEBUG
      if (wdebug > 0) std::cout << "doFixOrientation: innerPoints reversed: " << i - 1 << std::endl;
#endif
    }
  }
}

v8::Local<String> checkArguments(const Nan::FunctionCallbackInfo<v8::Value>& info, int32_t checkLength) {
  v8::Local<String> result = Nan::EmptyString();

  /* check info for wrong function call
   * info[0]: array of outerPoints and innerPoints arrays
   * info[1]: pointType integer || double -> IntPoint = doubleFactor * double
   * info[2]: Polygon shrink value (negative -> shrink; positive -> expand)
   * info[3]: optional: Jointype (jtMiter, jtSquare or jtRound)
   * info[4]: optional: double MiterLimit
   */
  if (info.Length() < 2) {
    result = v8String("Too few arguments! At least 'polyshape[][][]' and 'pointType' are required!");
    return result;
  }
  if (info.Length() < checkLength) {
    result = v8String("Too few arguments!");
    return result;
  }

  if (!info[0]->IsArray()) {
    result = String::Concat(v8String("Wrong argument 'polyshape': array[shapes][points][point] required: "), info[0]->ToString());
    return result;
  }

  if (checkLength < 2) {
    return result;
  }

  if (!info[1]->IsString() || !(info[1]->Equals(v8String("double")) || info[1]->Equals(v8String("integer")))) {
    result = String::Concat(v8String("Wrong argument 'pointType': 'double' || 'integer' required: "), info[1]->ToString());
    return result;
  }

  if ((info.Length() > 2) && (checkLength > 2)) {
    if (!info[2]->IsNumber()) {
      result = String::Concat(v8String("Wrong argument 'delta' || 'distance': number required: "), info[2]->ToString());
      return result;
    }
  }

  if ((info.Length() > 3) && (checkLength > 3)) {
    if (!info[3]->IsString() || !(info[3]->Equals(v8String("jtMiter")) || info[3]->Equals(v8String("jtSquare")) || info[3]->Equals(v8String("jtRound")))) {
      result = String::Concat(v8String("Wrong argument 'joinType': 'jtMiter' || 'jtSquare' || 'jtRound' required: "), info[3]->ToString());
      return result;
    }
  }

  if ((info.Length() > 4) && (checkLength > 4)) {
    if (!info[4]->IsNumber()) {
      result = String::Concat(v8String("Wrong argument 'miterLimit': number required: "), info[4]->ToString());
      return result;
    }
  }
  return result;
}

NAN_METHOD(setDebug) {
  // Try to parse first argument into int32_t, default to -1 if this fails
  int32_t maybeDebug = Nan::To<int32_t>(info[0]).FromMaybe(-1);
  if (maybeDebug >= 0) {
    wdebug = maybeDebug;
  }
  info.GetReturnValue().Set(Nan::New(wdebug));
}

NAN_METHOD(orientation) {
  bool doubleType = false;

  v8::Local<String> errMsg = checkArguments(info, 2);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }

  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  if (polyshape.size() <= 0) {
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  v8::Local<v8::Array> orientations = Nan::New<v8::Array>();
  for (unsigned int i = 0; i < polyshape.size(); i++) {
    bool polyOrientation = ClipperLib::Orientation(polyshape[i]);
    orientations->Set(orientations->Length(), Nan::New(polyOrientation));
  }

  info.GetReturnValue().Set(orientations);
}

NAN_METHOD(offset) {
  JoinType joinType = jtMiter;
  double miterLimit = 30.0;
  long delta;
  bool doubleType = false;

  v8::Local<String> errMsg = checkArguments(info, 3);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }

  if (doubleType) {
    delta = info[2]->NumberValue() * doubleFactor;
  } else {
    delta = info[2]->NumberValue();
  }
#ifdef DEBUG
  if (wdebug > 0) std::cout << "info[2]: delta: " << delta << std::endl;
#endif

  if (info.Length() > 3) {
    if (info[3]->Equals(v8String("jtMiter"))) {
      joinType = jtMiter;
    }
    if (info[3]->Equals(v8String("jtSquare"))) {
      joinType = jtSquare;
    }
    if (info[3]->Equals(v8String("jtRound"))) {
      joinType = jtRound;
    }
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[3]: joinType: " << *Nan::Utf8String(info[3]) << std::endl;
#endif
  }

  if (info.Length() > 4) {
    miterLimit = info[4]->NumberValue();
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[4]: miterLimit: " << miterLimit << std::endl;
#endif
  }

  if (!info[0]->IsArray()) {
    v8::Local<String> errMsg = v8String("Wrong argument - offset requires an array as input");
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  Paths polyshapeOut;

#ifdef DEBUG
  if (wdebug > 1) std::cout << "before Offset: polyshape.size(): " << polyshape.size() << std::endl;
#endif
  ClipperOffset co(miterLimit, doubleFactor / 10);
  co.AddPaths(polyshape, joinType, etClosedPolygon);
  co.Execute(polyshapeOut, delta);

#ifdef DEBUG
  if (wdebug > 1) std::cout << "after  Offset: polyshapeOut.size(): " << polyshapeOut.size() << std::endl;
#endif
  if (polyshapeOut.size() > 0) {
    info.GetReturnValue().Set(pathsToV8Array(polyshapeOut, doubleType));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(lineOffset) {

  JoinType joinType = jtMiter;
  double miterLimit = 30.0;
  long delta;
  bool doubleType = false;

  v8::Local<String> errMsg = checkArguments(info, 3);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }

  if (doubleType) {
    delta = info[2]->NumberValue() * doubleFactor;
  } else {
    delta = info[2]->NumberValue();
  }
#ifdef DEBUG
  if (wdebug > 0) std::cout << "info[2]: delta: " << delta << std::endl;
#endif

  if (info.Length() > 3) {
    if (info[3]->Equals(v8String("jtMiter"))) {
      joinType = jtMiter;
    }
    if (info[3]->Equals(v8String("jtSquare"))) {
      joinType = jtSquare;
    }
    if (info[3]->Equals(v8String("jtRound"))) {
      joinType = jtRound;
    }
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[3]: joinType: " << *Nan::Utf8String(info[3]) << std::endl;
#endif
  }

  if (info.Length() > 4) {
    miterLimit = info[4]->NumberValue();
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[4]: miterLimit: " << miterLimit << std::endl;
#endif
  }

  if (!info[0]->IsArray()) {
    v8::Local<String> errMsg = v8String("Wrong argument - lineOffset requires an array as input");
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  Paths polyshapeOut;

#ifdef DEBUG
  if (wdebug > 1) std::cout << "before Offset: polyshape.size(): " << polyshape.size() << std::endl;
#endif

  ClipperOffset co(miterLimit, 0.25);
  co.AddPaths(polyshape, joinType, etOpenButt);
  co.Execute(polyshapeOut, delta);

#ifdef DEBUG
  if (wdebug > 1) std::cout << "after  Offset: polyshapeOut.size(): " << polyshapeOut.size() << std::endl;
#endif
  if (polyshapeOut.size() > 0) {
    info.GetReturnValue().Set(pathsToV8Array(polyshapeOut, doubleType));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(unionArrays) {
  bool doubleType = true;
  ClipType clipType = ctUnion;

  v8::Local<String> errMsg = checkArguments(info, 1);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  v8::Local<v8::Array> inputArray = v8::Local<v8::Array>::Cast(info[0]);
  int len = inputArray->Length();

  //no polyshape with less than 1 polygon possible
  if (len < 1) {
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  Clipper clipper;

  for (int i = 0; i < len; i++) {
    if (!inputArray->Get(i)->IsArray()) continue;
    Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(inputArray->Get(i)), doubleType);
    if (polyshape.size() <= 0) {
      continue;
    }
    clipper.AddPaths(polyshape, ptSubject, true);
  }

  Paths clipperSolution;
  clipper.Execute(clipType, clipperSolution, pftNonZero, pftNonZero);

#ifdef DEBUG
  if (wdebug > 1) std::cout << "clipperSolution.size(): " << clipperSolution.size() << std::endl;
#endif

  v8::Local<v8::Array> solutions = Nan::New<v8::Array>();
  Paths singleSolution;
  unsigned int i = 0;
  while (i < clipperSolution.size()) {
    do {
      singleSolution.push_back(clipperSolution[i]);
      i++;
    } while (i < clipperSolution.size() && !ClipperLib::Orientation(clipperSolution[i]));
    solutions->Set(solutions->Length(), pathsToV8Array(singleSolution, doubleType));
    singleSolution.clear();
  }
#ifdef DEBUG
  if (wdebug > 1) std::cout << "solutions->Length(): " << solutions->Length() << std::endl;
#endif

  if (solutions->Length() > 0) {
    info.GetReturnValue().Set(solutions);
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(clip) {
  bool doubleType = false;
  ClipType clipType = ctIntersection;

  v8::Local<String> errMsg = checkArguments(info, 1);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[2]->Equals(v8String("double"))) {
    doubleType = true;
  }

  if (info.Length() > 3) {
    if (info[3]->Equals(v8String("ctUnion"))) {
      clipType = ctUnion;
    }
    if (info[3]->Equals(v8String("ctDifference"))) {
      clipType = ctDifference;
    }
    if (info[3]->Equals(v8String("ctXor"))) {
      clipType = ctXor;
    }
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[3]: clipType: " << *Nan::Utf8String(info[3]) << std::endl;
#endif
  }

  if (!info[0]->IsArray() || !info[1]->IsArray()) {
    v8::Local<String> errMsg = v8String("Wrong argument - clip requires an array as input");
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  Clipper clipper;

  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  clipper.AddPaths(polyshape, ptSubject, true);

  polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[1]), doubleType);
  clipper.AddPaths(polyshape, ptClip, true);

  Paths clipperSolution;

  clipper.Execute(clipType, clipperSolution, pftNonZero, pftNonZero);

#ifdef DEBUG
  if (wdebug > 1) std::cout << "clipperSolution.size(): " << clipperSolution.size() << std::endl;
#endif

  v8::Local<v8::Array> solutions = Nan::New<v8::Array>();
  Paths singleSolution;
  unsigned int i = 0;
  while (i < clipperSolution.size()) {
    do {
      singleSolution.push_back(clipperSolution[i]);
      i++;
    } while (i < clipperSolution.size() && !ClipperLib::Orientation(clipperSolution[i]));
    solutions->Set(solutions->Length(), pathsToV8Array(singleSolution, doubleType));
    singleSolution.clear();
  }
#ifdef DEBUG
  if (wdebug > 1) std::cout << "solutions->Length(): " << solutions->Length() << std::endl;
#endif

  if (solutions->Length() > 0) {
    info.GetReturnValue().Set(solutions);
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}


NAN_METHOD(clean) {
  bool doubleType = false;
  double distance = 1.415;

  v8::Local<String> errMsg = checkArguments(info, 2);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }

  if (info.Length() > 2) {
    distance = info[2]->NumberValue();
#ifdef DEBUG
    if (wdebug > 0) std::cout << "info[2]: distance: " << *Nan::Utf8String(info[2]) << std::endl;
#endif
  }

  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  Paths polyshapeOut(polyshape.size());

#ifdef DEBUG
  if (wdebug > 1) std::cout << "before CleanPolygons: polyshape.size(): " << polyshape.size() << std::endl;
#endif
  ClipperLib::CleanPolygons(polyshape, polyshapeOut, distance);
#ifdef DEBUG
  if (wdebug > 1) std::cout << "after  CleanPolygons: polyshapeOut.size(): " << polyshapeOut.size() << std::endl;
#endif

  if (polyshapeOut.size() > 0) {
    info.GetReturnValue().Set(pathsToV8Array(polyshapeOut, doubleType));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}


NAN_METHOD(fixOrientation) {
  bool doubleType = false;

  v8::Local<String> errMsg = checkArguments(info, 2);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }


  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  doFixOrientation(polyshape);

  if (polyshape.size() > 0) {
    info.GetReturnValue().Set(pathsToV8Array(polyshape, doubleType));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(simplify) {
  bool doubleType = false;

  v8::Local<String> errMsg = checkArguments(info, 2);
  if (errMsg->Length() > 0) {
    std::cout << *Nan::Utf8String(errMsg);
    Nan::ThrowTypeError(errMsg);
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  if (info[1]->Equals(v8String("double"))) {
    doubleType = true;
  }


  Paths polyshape = v8ArrayToPaths(v8::Local<v8::Array>::Cast(info[0]), doubleType);
  ClipperLib::SimplifyPolygons(polyshape, polyshape, pftNonZero);

  if (polyshape.size() > 0) {
    info.GetReturnValue().Set(pathsToV8Array(polyshape, doubleType));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_MODULE_INIT(init) {
  Nan::Set(
    target,
    v8String("setDebug"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(setDebug)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("orientation"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(orientation)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("offset"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(offset)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("lineOffset"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(lineOffset)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("unionArrays"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(unionArrays)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("clip"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(clip)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("clean"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(clean)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("fixOrientation"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(fixOrientation)).ToLocalChecked()
  );
  Nan::Set(
    target,
    v8String("simplify"),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(simplify)).ToLocalChecked()
  );
}

NODE_MODULE(clipper, init)
