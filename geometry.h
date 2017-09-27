#ifndef GEOMETRY_H
#define GEOMETRY_H

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

  vect2Di operator- ()
  {
    vect2Di c;
    c.x = -this->x;
    c.y = -this->y;
    return c;
  }
  
  vect2Di operator* (mat2Di M);

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
    this->x=b.x;
    this->y=b.y;
  }

};

struct mat2Di
{
  int m11, m12, m21, m22;

  mat2Di operator+ (mat2Di b)
  {
    mat2Di c;
    c.m11 = this->m11 + b.m11;
    c.m12 = this->m12 + b.m12;
    c.m21 = this->m21 + b.m21;
    c.m22 = this->m22 + b.m22;
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
};

vect2Di vect2Di::operator* (struct mat2Di M)
{
  vect2Di c;
  c.x = this->x * M.m11 + this->y * M.m12;
  c.y = this->x * M.m21 + this->y * M.m22;
  return c;
}

#endif
