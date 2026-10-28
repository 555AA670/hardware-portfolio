#ifndef SIM_CANDIDATE_CONTROL_H
#define SIM_CANDIDATE_CONTROL_H
#include "control.h"
/* Simulation candidate only. Not linked into or downloaded as board firmware. */
typedef struct {
 CleanControl pi;
 float filtered_vin,feedforward;
 float previous_voltage,voltage_rate;
 uint32_t hold,updates,prelimit,rise_limited,command,light_load,low_load_count;
 uint32_t current_cap,soft_current_active;
} CandidateControl;
#define CANDIDATE_MAX_EFFECTIVE_COUNTS 24480U /* proven open-loop nominal 90% */
typedef struct {float kp,ki,rff,soft_high,soft_hold,soft_release;uint32_t slew,backoff,release;float damping_resistance;} CandidateParameters;
/* Runtime tunable ONLY in the simulation search executable. */
extern CandidateParameters candidate_parameters;
void Candidate_Reset(CandidateControl *c,float output);
uint32_t Candidate_Sample(CandidateControl *c,float measured,float vin,float iout,float iin);
uint32_t Candidate_Pulse(uint32_t request,uint32_t enabled,uint32_t *credit);
uint32_t Candidate_Output(const CandidateControl *c,uint32_t delayed_request);
#endif
