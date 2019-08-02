/**
 * Let `S` be a symmetry (represented by its corresponding `CubieCube`) and `C` a coordinate (also in form of some
 * `CubieCube`).
 *
 * Then we can apply `S` to `C` as `S^-1 * C * S` to get the symmetric coordinate `C'`. A sym-coordinate $B$ represents
 * all symmetric coordinates that can be generated by `T^-1 * B * T` for some symmetry `T`. `B` is then called
 * the representative of this symmetry class, its coordinate is the index of the class. A symmetry `S` is a
 * self-symmetry of class `B` if `S^-1 * B * S = B`, i.e. it does not generate any new coordinate of its class.
 *
 * The conjugate `C'` of some coordinate `C` is computed by `S * C * S^-1`, which means that applying symmetry `S` to
 * `C'` yields the original `C`.
 *
 * The application of the concepts mentioned here is detailed in "prun.c".
 */

#include "sym.h"
#include <stdint.h>
#include <algorithm>

#define EMPTY ~uint32_t(0) // to indicate that a raw-coordinate has not been assigned to its sym-coordinate yet

CubieCube sym_cubes[N_SYMS];
int inv_sym[N_SYMS];
int conj_move[N_MOVES][N_SYMS];

uint16_t (*conj_twist)[N_SYMS_SUB];
uint16_t (*conj_udedges)[N_SYMS_SUB];

uint32_t *fslice_sym;
uint32_t *cperm_sym;
uint32_t *fslice_raw;
uint16_t *cperm_raw;
uint16_t *fslice_selfs;
uint16_t *cperm_selfs;

/*
 * Calling this function initializes the basic tables: `sym_cubes`, `inv_sym` and `conj_move`. Notice how the first
 * 4 symmetries are the ones usable in 5-face mode, hence we can easily adapt the table generation accordingly by just
 * setting `N_SYMS_SUB = 4`. As we need to perform some operations for symmetries with index larger than `N_SYMS_SUB`
 * we simply generate the information for all 48 of them (these tables are very small anyways).
 */
void initSym() {
  CubieCube cube;
  CubieCube tmp;

  cube = kSolvedCube;
  for (int i = 0; i < N_SYMS; i++) {
    sym_cubes[i] = cube;

    mul(cube, kLR2Cube, tmp);
    std::swap(tmp, cube);

    if (i % 2 == 1) {
      mul(cube, kF2Cube, tmp);
      std::swap(tmp, cube);
    }
    if (i % 4 == 3) {
      mul(cube, kU4Cube, tmp);
      std::swap(tmp, cube);
    }
    if (i % 16 == 15) {
      mul(cube, kURF3Cube, tmp);
      std::swap(tmp, cube);
    }
  }

  /*
   * The following code-blocks are quite inefficient with an extra loop each, however these tables are so small
   * compared to everything else which we need to load on start-up that we just keep it as straight-forward as
   * possible here.
   */

  for (int i = 0; i < N_SYMS; i++) {
    for (int j = 0; j < N_SYMS; j++) {
      mul(sym_cubes[i], sym_cubes[j], cube);
      if (cube == kSolvedCube) {
        inv_sym[i] = j;
        break;
      }
    }
  }

  for (int m = 0; m < N_MOVES + N_DOUBLE2; m++) {
    for (int s = 0; s < N_SYMS; s++) {
      mul(sym_cubes[s], move_cubes[m], tmp);
      mul(tmp, sym_cubes[inv_sym[s]], cube);
      for (int conj = 0; conj < N_MOVES; conj++) {
        if (cube == move_cubes[conj]) {
          conj_move[m][s] = conj;
          break;
        }
      }
    }
  }
}

// Computes the `conj_*` table for a coordinate
void initConjCoord(
  uint16_t (**conj_coord)[N_SYMS_SUB],
  int n_coords,
  int (*getCoord)(const CubieCube &),
  void (*setCoord)(CubieCube &, int),
  void (*mul)(const CubieCube &, const CubieCube &, CubieCube &)
) {
  auto conj_coord1 = new uint16_t[n_coords][N_SYMS_SUB];

  CubieCube cube1 = kSolvedCube; // make sure that all multiplications will work;
  CubieCube cube2;
  CubieCube tmp;
  for (int c = 0; c < n_coords; c++) {
    setCoord(cube1, c);
    conj_coord1[c][0] = c; // symmetry 0 is just the identity
    for (int s = 1; s < N_SYMS_SUB; s++) {
      mul(sym_cubes[s], cube1, tmp);
      mul(tmp, sym_cubes[inv_sym[s]], cube2);
      conj_coord1[c][s] = getCoord(cube2);
    }
  }

  *conj_coord = conj_coord1;
}

/*
 * Generates all sym-tables for the FSLICE coordinate. Note that we want a double loop here for efficiency and
 * for that reason (and some datatype inconsistencies) cannot easily share code with `initCPermTables()`.
 */
void initFSliceTables() {
  fslice_sym = new uint32_t[N_FSLICE];
  fslice_raw = new uint32_t[N_FSLICE_SYM];
  fslice_selfs = new uint16_t[N_FSLICE_SYM];
  std::fill(fslice_sym, fslice_sym + N_FSLICE, EMPTY);

  CubieCube cube1 = kSolvedCube;;
  CubieCube cube2;
  CubieCube tmp;
  int cls = 0;

  for (int slice = 0; slice < N_SLICE; slice++) {
    setSlice(cube1, slice); // SLICE is slightly more expensive to set, hence we do it in the outer loop
    for (int flip = 0; flip < N_FLIP; flip++) {
      setFlip(cube1, flip);
      int fslice = FSLICE(flip, slice);

      if (fslice_sym[fslice] != EMPTY)
        continue;

      fslice_sym[fslice] = SYMCOORD(cls, 0);
      fslice_raw[cls] = fslice;
      fslice_selfs[cls] = 1; // symmetry 0 is just the identity, thereby always a self-sym

      for (int s = 1; s < N_SYMS_SUB; s++) {
        mulEdges(sym_cubes[inv_sym[s]], cube1, tmp);
        mulEdges(tmp, sym_cubes[s], cube2);
        int fslice1 = getFSlice(cube2);
        if (fslice_sym[fslice1] == EMPTY)
          fslice_sym[fslice1] = SYMCOORD(cls, s);
        else if (fslice1 == fslice) // collect self-symmetries here essentially for free
          fslice_selfs[cls] |= 1 << s;
      }
      cls++;
    }
  }
}

// Generates all sym-tables for the CPERM coordinate
void initCPermTables() {
  cperm_sym = new uint32_t[N_CPERM];
  cperm_raw = new uint16_t[N_CPERM_SYM];
  cperm_selfs = new uint16_t[N_CPERM_SYM];
  std::fill(cperm_sym, cperm_sym + N_CPERM, EMPTY);

  CubieCube cube1;
  CubieCube cube2;
  CubieCube tmp;
  int cls = 0;

  cube1 = kSolvedCube;
  for (int cperm = 0; cperm < N_CPERM; cperm++) {
    setCPerm(cube1, cperm);

    if (cperm_sym[cperm] != EMPTY)
      continue;

    cperm_sym[cperm] = SYMCOORD(cls, 0);
    cperm_raw[cls] = cperm;
    cperm_selfs[cls] = 1;

    for (int s = 1; s < N_SYMS_SUB; s++) {
      mulCorners(sym_cubes[inv_sym[s]], cube1, tmp);
      mulCorners(tmp, sym_cubes[s], cube2);
      int cperm1 = getCPerm(cube2);
      if (cperm_sym[cperm1] == EMPTY)
        cperm_sym[cperm1] = SYMCOORD(cls, s);
      else if (cperm1 == cperm)
        cperm_selfs[cls] |= 1 << s;
    }
    cls++;
  }
}

// Initializes all conjugation- and symmetry-tables
void initSymTables() {
  initConjCoord(&conj_twist, N_TWIST, getTwist, setTwist, mulCorners);
  initConjCoord(&conj_udedges, N_UDEDGES2, getUDEdges2, setUDEdges2, mulEdges);
  initFSliceTables();
  initCPermTables();
}
