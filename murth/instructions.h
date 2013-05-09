#pragma once
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

//! get instruction function pointer
//! \param name instruction name
//! \returns instruction function pointer if instruction with than name exists
//! \returns NULL if there's no such instruction
murth_instr_func_t murth_instr_get_func(const char *name);

//! get instruction name
//! \param index instruction index
//! \returns instruction name for index
//! \returns NULL if index is larger than instructions count
const char *murth_instr_get_name(int index);

//! register custom instruction
//! \param name name of the new instruction
//! \param func function of the new instruction
//! \returns index if function was succesfully registered
//! \returns 0 if max number of possible instruction reached
//! \returns -1 if instruction with the same name already exists
int murth_instr_register(const char *name, murth_instr_func_t func);

#ifdef __cplusplus
}
#endif

