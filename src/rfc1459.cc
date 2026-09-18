/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * rfc1459.cc
 *
 */


#include "common.h"
#include "rfc1459.h"

/* Global instances */
Rfc1459Mapping rfc1459_mapping;
AsciiMapping ascii_mapping;
CaseMapping *active_case_mapping = &rfc1459_mapping;

/* C API function pointers - initialized to RFC1459 implementations */
static int c_rfc_casecmp(const char *s1, const char *s2) {
  return active_case_mapping->casecmp(s1, s2);
}
static int c_rfc_ncasecmp(const char *s1, const char *s2, size_t n) {
  return active_case_mapping->ncasecmp(s1, s2, n);
}
static bool c_rfc_char_equal(const char c1, const char c2) {
  return active_case_mapping->char_equal(c1, c2);
}

int (*rfc_casecmp)(const char *, const char *) = c_rfc_casecmp;
int (*rfc_ncasecmp)(const char *, const char *, size_t) = c_rfc_ncasecmp;
bool (*rfc_char_equal)(const char, const char) = c_rfc_char_equal;

/* Standalone C functions for direct callers */
int _rfc_casecmp(const char *s1, const char *s2)
{
  while ((*s1) && (*s2) && active_case_mapping->char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

int _rfc_ncasecmp(const char *s1, const char *s2, size_t n)
{
  if (!n)
    return 0;
  while (--n && (*s1) && (*s2) && active_case_mapping->char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

/* Rfc1459Mapping implementation */
int Rfc1459Mapping::casecmp(const char *s1, const char *s2) const
{
  while ((*s1) && (*s2) && char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

int Rfc1459Mapping::ncasecmp(const char *s1, const char *s2, size_t n) const
{
  if (!n)
    return 0;
  while (--n && (*s1) && (*s2) && char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

bool Rfc1459Mapping::char_equal(char c1, char c2) const
{
  return _rfc_toupper(c1) == _rfc_toupper(c2);
}

/* AsciiMapping implementation */
int AsciiMapping::casecmp(const char *s1, const char *s2) const
{
  return strcasecmp(s1, s2);
}

int AsciiMapping::ncasecmp(const char *s1, const char *s2, size_t n) const
{
  return strncasecmp(s1, s2, n);
}

bool AsciiMapping::char_equal(char c1, char c2) const
{
  return c1 == c2;
}

/* vim: set sts=2 sw=2 ts=8 et: */
