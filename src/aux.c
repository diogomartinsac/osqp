#include "aux.h"
#include "util.h"

/***********************************************************
 * Auxiliary functions needed to compute ADMM iterations * *
 ***********************************************************/

/**
 * Cold start workspace variables
 * @param work Workspace
 */
void cold_start(Work *work) {
    memset(work->x, 0, (work->data->n + work->data->m) * sizeof(c_float));
    memset(work->z, 0, (work->data->n + work->data->m) * sizeof(c_float));
    memset(work->u, 0, (work->data->n + work->data->m) * sizeof(c_float));
}


/**
 * Update RHS during first tep of ADMM iteration (store it into x)
 * @param  work Workspace
 */
void compute_rhs(Work *work){
    c_int i; // Index
    for (i=0; i < work->data->n; i++){
        // Cycle over part related to original x variables
        work->x[i] = work->settings->rho * (work->z[i] - work->u[i])
                       - work->data->q[i];
    }
    for (i = work->data->n; i < work->data->n + work->data->m; i++){
        // Cycle over dual variable within first step (nu)
        work->x[i] = work->z[i] - work->u[i];
    }

}


/**
 * Update x variable (slacks s related part)
 * after solving linear system (first ADMM step)
 *
 * @param work Workspace
 */
void update_x(Work *work){
    c_int i; // Index
    for (i = work->data->n; i < work->data->n + work->data->m; i++){
        work->x[i] = 1./work->settings->rho * work->x[i] + work->z[i] - work->u[i];
        //TODO: Remove 1/rho operation (store 1/rho during setup)
    }
}


/**
 * Project x (second ADMM step)
 * @param work Workspace
 */
void project_x(Work *work){
    c_int i;
    for (i = 0; i < work->data->n; i++){
        // Part related to original x variables
        work->z[i] = c_min(c_max(work->settings->alpha * work->x[i] +
                     (1.0 - work->settings->alpha) * work->z_prev[i] +
                     work->u[i], work->data->lx[i]), work->data->ux[i]);
    }

    for (i = work->data->n; i < work->data->n + work->data->m; i++){
        // Part related to slack variables
        work->z[i] = c_min(c_max(work->settings->alpha * work->x[i] +
                     (1.0 - work->settings->alpha) * work->z_prev[i] +
                     work->u[i], work->data->lA[i - work->data->n]), work->data->uA[i - work->data->n]);
    }

}

/**
 * Update u variable (third ADMM step)
 * @param work Workspace
 */
void update_u(Work *work){
    c_int i; // Index
    for (i = 0; i < work->data->n + work->data->m; i++){
        work->u[i] += work->settings->alpha * work->x[i] +
                      (1.0 - work->settings->alpha) * work->z_prev[i] -
                      work->z[i];
    }

}

/**
 * Compute objective function from data at value x
 * @param  data Data structure
 * @param  x    Value x
 * @return      Objective function value
 */
c_float compute_obj_val(Data * data, c_float * x){
    return quad_form(data->P, x) + vec_prod(data->q, x, data->n);
}


/**
 * Return norm of primal residual
 * TODO: Use more tailored residual (not general one)
 * @param  work Workspace
 * @return      Norm of primal residual
 */
c_float compute_pri_res(Work * work){
    return vec_norm2_diff(work->x, work->z, work->data->n);
}



/**
 * Return norm of dual residual
 * TODO: Use more tailored residual (not general one)
 * @param  work Workspace
 * @return      Norm of dual residual
 */
c_float compute_dua_res(Work * work){
    c_int i;
    c_float norm_sq = 0, temp;
    for (i = 0; i < work->data->n; i++){
        temp = work->settings->rho * (work->z[i] - work->z_prev[i]);
        norm_sq += temp*temp;
    }
    return c_sqrt(norm_sq);
}



/**
 * Update solver information
 * @param work Workspace
 */
void update_info(Work *work, c_int iter){
    work->info->iter = iter; // Update iteration number
    work->info->obj_val = compute_obj_val(work->data, work->z);
    work->info->pri_res = compute_pri_res(work);
    work->info->dua_res = compute_dua_res(work);
}


/**
 * Check if residuals norm meet the required tolerance
 * @param  work Workspace
 * @return      Redisuals check
 */
c_int residuals_check(Work *work){
    c_float eps_pri, eps_dua;
    c_int exitflag = 0;
    //TODO: Remove sqrt(n+m) computation (precompute at the beginning)

    eps_pri = c_sqrt(work->data->n + work->data->m) * work->settings->eps_abs +
              work->settings->eps_rel * c_max(vec_norm2(work->x, work->data->n + work->data->m),
                                              vec_norm2(work->z, work->data->n + work->data->m));
    eps_dua = c_sqrt(work->data->n + work->data->m) * work->settings->eps_abs +
              work->settings->eps_rel * work->settings->rho * vec_norm2(work->u, work->data->n + work->data->m);

    if (work->info->pri_res < eps_pri && work->info->dua_res < eps_dua) exitflag = 1;

    return exitflag;

}
