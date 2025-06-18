#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <dirent.h>


#define BUF_SIZE 1024

void executer_dir(const char *chemin) {
    // Utilise le chemin fourni ou '.' (répertoire courant) si aucun chemin n'est donné
    const char *target = chemin ? chemin : ".";
    
    // Ouvre le répertoire spécifié
    DIR *d = opendir(target);
    if (!d) {
        // Si l'ouverture échoue, affiche une erreur système
        perror("erreur ouverture répertoire");
        return;
    }

    struct dirent *entree;
    // Lit chaque entrée du répertoire une par une jusqu'à la fin
    while ((entree = readdir(d)) != NULL) {
        // Affiche le nom de l'entrée (fichier ou sous-répertoire)
        printf("%s\n", entree->d_name);
    }

    // Ferme le flux de répertoire
    if (closedir(d) == -1) {
        // Si la fermeture échoue, affiche une erreur système
        perror("erreur fermeture répertoire");
    }
}

/*   Etape 16
void executer_pipeline(struct cmdline *commande) {
    int tube[2];
    if (pipe(tube) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Fils 1 : commande gauche
        close(tube[0]); // ferme lecture
        dup2(tube[1], STDOUT_FILENO); // redirige stdout vers tube
        close(tube[1]);
        execvp(commande->seq[0][0], commande->seq[0]);
        perror("execvp 1");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Fils 2 : commande droite
        close(tube[1]); // ferme écriture
        dup2(tube[0], STDIN_FILENO); // redirige stdin depuis tube
        close(tube[0]);
        execvp(commande->seq[1][0], commande->seq[1]);
        perror("execvp 2");
        exit(EXIT_FAILURE);
    }

    // Père
    close(tube[0]);
    close(tube[1]);
    wait(NULL);
    wait(NULL);
}
*/

void executer_pipeline(struct cmdline *commande) {
    int n = 0;
    while (commande->seq[n] != NULL) n++;

    int i;
    int tube_prec[2] = {-1, -1}; // tube précédent
    for (i = 0; i < n; i++) {
        int tube_suiv[2];

        if (i < n - 1) {
            if (pipe(tube_suiv) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Fils

            // Si pas le premier, on redirige l'entrée depuis tube précédent
            if (i > 0) {
                dup2(tube_prec[0], STDIN_FILENO);
                close(tube_prec[0]);
                close(tube_prec[1]);
            }

            // Si pas le dernier, on redirige la sortie vers tube suivant
            if (i < n - 1) {
                close(tube_suiv[0]);
                dup2(tube_suiv[1], STDOUT_FILENO);
                close(tube_suiv[1]);
            }

            execvp(commande->seq[i][0], commande->seq[i]);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        // Père : ferme les tubes qu’il n’utilise plus
        if (i > 0) {
            close(tube_prec[0]);
            close(tube_prec[1]);
        }
        if (i < n - 1) {
            tube_prec[0] = tube_suiv[0];
            tube_prec[1] = tube_suiv[1];
        }
    }

    // Attend tous les fils
    for (i = 0; i < n; i++) {
        wait(NULL);
    }
}


/* traitement associe a un signal*/
void traitement(int sig){
    int status;
    pid_t pid;
    if (sig == SIGCHLD){
        while ((pid= waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
            if (WIFEXITED(status)) {
                printf("Processus %d terminé avec code %d\n", pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Processus %d tué par signal %d\n", pid, WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                printf("Processus %d stoppé (CTRL+Z ?) signal %d\n", pid, WSTOPSIG(status));
            } else if (WIFCONTINUED(status)) {
                printf("Processus %d repris\n", pid);
            }
        }
    }
    
    /* etape 11.1
    else if (sig == SIGTSTP){
        printf("\nsignal SIGTSTP reçu\n");
        printf("> ");
    }
    else if (sig == SIGINT){
        printf("\nsignal SIGINT reçu\n");
        printf("> ");
    }
    */    
    
    printf("signal recu: %d\n", sig);
}

int main(void) {
    bool fini= false;

    /*definition d'une variable action*/
    struct sigaction action;
    action.sa_handler = traitement;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    
    /*association du signal a l'action*/
    if (sigaction(SIGCHLD, &action, NULL)== -1) {
        exit(EXIT_FAILURE);
    }
    
    /* etape 11.1
    else if (sigaction(SIGTSTP, &action, NULL)== -1) {
        exit(EXIT_FAILURE);
    }

    else if (sigaction(SIGINT, &action, NULL)== -1) {
        exit(EXIT_FAILURE);
    }
    */
    
    //Etape 11.3
    sigset_t masque;
    sigemptyset(&masque); // notre set <- 0
    sigaddset(&masque, SIGINT); // ajouter Ctrl+C au set
    sigaddset(&masque, SIGTSTP); // ajouter Ctrl+Z  au set

    if (sigprocmask(SIG_BLOCK, &masque, NULL) == -1) {
        perror("erreur de sigprocmask");
        exit(EXIT_FAILURE);
    }




    while (!fini) {
        printf("> ");
        struct cmdline *commande= readcmd();

        if (commande == NULL) {
            // commande == NULL -> erreur readcmd()
            perror("erreur lecture commande \n");
            exit(EXIT_FAILURE);
    
        } else {

            if (commande->err) {
                // commande->err != NULL -> commande->seq == NULL
                printf("erreur saisie de la commande : %s\n", commande->err);
        
            } else {

                /* Pour le moment le programme ne fait qu'afficher les commandes 
                   tapees et les affiche à l'écran. 
                   Cette partie est à modifier pour considérer l'exécution de ces
                   commandes 
                */
                int indexseq= 0;
                char **cmd;
                
                if (commande->seq[1] != NULL) {
                    // Si plusieurs commandes reliées : pipeline
                    executer_pipeline(commande);
                    continue;
                }
                

                while ((cmd= commande->seq[indexseq])) {
                    if (cmd[0]) {
                        if (strcmp(cmd[0], "exit") == 0) {
                            fini= true;
                            printf("Au revoir ...\n");
                        }
                        else if (strcmp(cmd[0], "dir") == 0) {
                            const char *chemin = cmd[1] ? cmd[1] : ".";
                            executer_dir(chemin);
                            indexseq++;
                            continue;
                        }                        
                        else if (strcmp(cmd[0], "cd") == 0) {
                            const char *path = cmd[1] ? cmd[1] : getenv("HOME");
                            if (chdir(cmd[1]) == -1) {
                                perror("erreur de chdir");
                            }
                            indexseq++;
                            continue;
                        }
                        else {
                            /* Etape 11.2
                            if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
                                perror("erreur de signal pour SIGINT");
                                exit(EXIT_FAILURE);
                            }

                            if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
                                perror("erreur de signal pour SIGTSTP");
                                exit(EXIT_FAILURE);
                            }
                            */


                            //printf("commande : ");
                            int indexcmd= 0;
                            pid_t pid_commande = fork();
                            if (pid_commande > 0) {
                                // Processus père
                                //int status;
                                if (commande->backgrounded == NULL) /*waitpid(pid_commande, &status, 0)*/ pause();
                            } else if (pid_commande == 0) {
                                // processus fils

                                /* Etape 11.2
                                 pour que le traitement du père ne soit pas
                                 hérité par le fils, on le redéfinit pour un
                                 traitement par défaut
                                if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                                    perror("erreur de signal pour SIGINT");
                                    exit(EXIT_FAILURE);
                                }

                                if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
                                    perror("erreur de signal pour SIGTSTP");
                                    exit(EXIT_FAILURE);
                                } */
                                
                                // Etape 11.3
                                sigset_t masque_fils;
                                sigemptyset(&masque_fils);
                                
                                if (sigprocmask(SIG_SETMASK, &masque_fils, NULL) == -1) {
                                    perror("erreur démasquage signaux dans le fils");
                                    exit(EXIT_FAILURE);
                                }
                                                              
                                // Etape 12
                                // les processus en arrière plan doivent etre
                                // insensibles
                                if (commande->backgrounded != NULL) {
                                    // mettre les processus en arrière plan dans
                                    // un nouveau groupe
                                    if (setpgrp() == -1) {
                                        perror("erreur setpgrp");
                                        exit(EXIT_FAILURE);
                                    }
                                }
                                if (commande -> in != NULL) {
                                    int fd_source; 
                                    if ( (fd_source= open(commande->in, O_RDONLY)) == -1) {
                                        perror("erreur ouverture fichier source");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (dup2(fd_source, 0) == -1) {
                                        perror("erreur redirection de l'entrée standard");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (close(fd_source) == -1) {
                                        perror("erreur fermeture fichier source");
                                        exit(EXIT_FAILURE);
                                    }
                                }

                                if (commande -> out!= NULL) {
                                    int fd_destination;
                                    if ( (fd_destination= open(commande->out, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
                                        perror("erreur ouverture fichier destination");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (dup2(fd_destination, 1) == -1) {
                                        perror("erreur redirection de la sortie standard");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (close(fd_destination) == -1) {
                                        perror("erreur fermeture fichier destination");
                                        exit(EXIT_FAILURE);
                                    }
                                }

                                execvp(cmd[0], cmd);
                                // Si execvp retourne, cad qu'elle a échoué
                                perror("erreur execvp");
                                exit(EXIT_FAILURE);
                            } else {
                                // Echec du fork
                                perror("erreur fork");
                            }
                            
                            
                            while (cmd[indexcmd]) {
                                printf("%s ", cmd[indexcmd]);
                                indexcmd++;
                            }
                            printf("\n");
                        }

                        indexseq++;
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
