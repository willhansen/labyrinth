#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <cmath>

struct mat2Di;

struct vect2Di
{
  int x;
  int y;

  vect2Di(int a, int b)
  {
    this->x = a;
    this->y = b;
  }

  vect2Di()
  {
    this->x = 0;
    this->y = 0;
  }

  vect2Di operator+ (vect2Di b)
  {
    vect2Di c;
    c.x = this->x + b.x;
    c.y = this->y + b.y;
    return c;
  }

  double angle()
  {
    return std::atan2(y, x);
  }

  vect2Di operator- ()
  {
    vect2Di c;
    c.x = -this->x;
    c.y = -this->y;
    return c;
  }
  
  vect2Di operator* (mat2Di M);
  void operator*= (mat2Di M);

  vect2Di operator- (vect2Di b)
  {
    return (*this)+(-b);
  }

  bool operator== (vect2Di b)
  {
    return (this->x==b.x) && (this->y==b.y);
  }
  
  bool operator!= (vect2Di b)
  {
    return !(*this == b);
  }

  void operator+= (vect2Di b)
  {
    this->x=b.x+this->x;
    this->y=b.y+this->y;
  }

  void operator-= (vect2Di b)
  {
    *this += -b;
  }

};

struct mat2Di
{
  int m11, m12, m21, m22;

  mat2Di(int m11, int m12, int m21, int m22)
    : m11(m11)
    , m12(m12)
    , m21(m21)
    , m22(m22)
  {}

  mat2Di()
  {
    this->m11 = 1;
    this->m12 = 0;
    this->m21 = 0;
    this->m22 = 1;
  }

  mat2Di operator+ (mat2Di b)
  {
    mat2Di c;
    c.m11 = this->m11 + b.m11;
    c.m12 = this->m12 + b.m12;
    c.m21 = this->m21 + b.m21;
    c.m22 = this->m22 + b.m22;
    return c;
  }

  mat2Di inversed()
  {
    mat2Di c;
    int det = m11*m22-m12*m21;
    c.m11 = m22/det;
    c.m22 = m11/det;
    c.m12 = -m12/det;
    c.m21 = -m21/det;
    return c;
  }

  mat2Di operator- ()
  {
    mat2Di c;
    c.m11 = -this->m11;
    c.m12 = -this->m12;
    c.m21 = -this->m21;
    c.m22 = -this->m22;
    return c;
  }

  mat2Di operator- (mat2Di b)
  {
    return (*this)+(-b);
  }

  vect2Di operator* (vect2Di a)
  {
    vect2Di c;
    c.x = a.x * this->m11 + a.y * this->m21;
    c.y = a.x * this->m12 + a.y * this->m22;
    return c;
  }

  mat2Di operator* (mat2Di b)
  {
    mat2Di c;
    c.m11 = m11*b.m11 + m12*b.m21;
    c.m12 = m11*b.m12 + m12*b.m22;
    c.m21 = m21*b.m11 + m22*b.m21;
    c.m22 = m21*b.m12 + m22*b.m22;
    return c;
  }

  void operator*= (mat2Di b)
  {
    mat2Di c;
    c.m11 = m11*b.m11 + m12*b.m21;
    c.m12 = m11*b.m12 + m12*b.m22;
    c.m21 = m21*b.m11 + m22*b.m21;
    c.m22 = m21*b.m12 + m22*b.m22;
    m11=c.m11;
    m12=c.m12;
    m21=c.m21;
    m22=c.m22;
  }
};

vect2Di vect2Di::operator* (struct mat2Di M)
{
  vect2Di c;
  c.x = this->x * M.m11 + this->y * M.m12;
  c.y = this->x * M.m21 + this->y * M.m22;
  return c;
}
void vect2Di::operator*= (struct mat2Di M)
{
  vect2Di c = *this * M;
  this->x = c.x;
  this->y = c.y;
}

const vect2Di LEFT = vect2Di(-1, 0);
const vect2Di RIGHT = vect2Di(1, 0);
const vect2Di UP = vect2Di(0, 1);
const vect2Di DOWN = vect2Di(0, -1);
const vect2Di ZERO = vect2Di(0, 0);

const mat2Di IDENTITY = mat2Di(1,0,0,1);
const mat2Di CCW = mat2Di(0,1,-1,0);
const mat2Di CW = mat2Di(0,-1,1,0);
const mat2Di FLIP_X = mat2Di(-1,0,0,1);
const mat2Di FLIP_Y = mat2Di(1,0,0,-1);



#endif
