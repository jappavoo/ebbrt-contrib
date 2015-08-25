#define NOMINMAX
#define _USE_MATH_DEFINES

#include <irtkReconstructionGPU.h>
#include <irtkResampling.h>
#include <irtkRegistration.h>
#include <irtkImageRigidRegistration.h>
#include <irtkImageRigidRegistrationWithPadding.h>
#include <irtkImageFunction.h>
#include <irtkTransformation.h>
#include <math.h>
#include <stdlib.h>
#include "utils.h"

class ParallelStackRegistrations {
  irtkReconstruction *reconstructor;
  vector<irtkRealImage>& stacks;
  vector<irtkRigidTransformation>& stack_transformations;
  int templateNumber;
  irtkGreyImage& target;
  irtkRigidTransformation& offset;
  bool _externalTemplate;
  public:
  ParallelStackRegistrations(irtkReconstruction *_reconstructor,
			     vector<irtkRealImage>& _stacks,
			     vector<irtkRigidTransformation>& _stack_transformations,
			     int _templateNumber,
			     irtkGreyImage& _target,
			     irtkRigidTransformation& _offset,
			     bool externalTemplate = false) :
    reconstructor(_reconstructor),
    stacks(_stacks),
    stack_transformations(_stack_transformations),
    target(_target),
    offset(_offset) {
    templateNumber = _templateNumber,
	_externalTemplate = externalTemplate;
  }
  void operator() (const blocked_range<size_t> &r) const;
  // execute
  void operator() () const;
};
