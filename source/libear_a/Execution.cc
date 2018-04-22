/*  Copyright (C) 2012-2017 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libear_a/Execution.h"

#include "libear_a/Array.h"

namespace {

    class ExecutionSerializer : public ear::Serializable {
    public:
        using Estimator = std::function<size_t()>;
        using Copier = std::function<char const **(char const **, char const **)>;

    public:
        ExecutionSerializer(Estimator const &estimator, Copier const &copier) noexcept
                : estimator_(estimator), copier_(copier) {}

        size_t estimate() const noexcept override {
            return estimator_();
        }

        const char **copy(char const **begin, char const **end) const noexcept override {
            return copier_(begin, end);
        }

    private:
        Estimator const &estimator_;
        Copier const &copier_;
    };


    int forward(ear::Serializable const &session,
                ear::Serializable const &execution,
                std::function<int(const char *, const char **)> const &function) noexcept {
        size_t const size = session.estimate() + execution.estimate();
        char const *dst[size];
        char const **it = dst;
        char const **const end = it + size;

        it = session.copy(it, end);
        it = execution.copy(it, end);

        return function(dst[0], dst);
    }

    ear::Result<int> forward(ear::Resolver const &linker,
                             ear::Serializable const &session,
                             ear::Serializable const &execution,
                             char const **envp) noexcept {
        return linker.execve()
                .map<int>([&session, &execution, &envp](auto fp) {
                    return ::forward(session,
                                     execution,
                                     [envp, &fp](auto cmd, auto args) {
                                         return fp(cmd,
                                                   const_cast<char *const *>(args),
                                                   const_cast<char *const *>(envp));
                                     });
                });
    }

}


namespace ear {

    Result<int> Execution::apply(Resolver const &linker, State const *state) noexcept {
        return (state == nullptr)
               ? this->apply(linker)
               : this->apply(linker, LibrarySessionSerializer(state->get_input()));
    }

    Execve::Execve(const char *path, char *const *argv, char *const *envp) noexcept
            : path_(path)
            , argv_(const_cast<const char **>(argv))
            , envp_(const_cast<const char **>(envp))
    { }

    Result<int> Execve::apply(Resolver const &linker) noexcept {
        return linker.execve()
                .map<int>([this](auto fp) {
                    return fp(path_,
                              const_cast<char *const *>(argv_),
                              const_cast<char *const *>(envp_));
                });
    }

    Result<int> Execve::apply(Resolver const &linker, Serializable const &session) noexcept {
        ExecutionSerializer execution(
                [this]() {
                    return ::ear::array::length(argv_) + 2;
                },
                [this](auto begin, auto end) {
                    const char **argv_begin = argv_;
                    const char **argv_end = argv_begin + ::ear::array::length(argv_);

                    auto it = begin;
                    *it++ = command_separator;
                    return ::ear::array::copy(argv_begin, argv_end, it, end);
                }
        );
        return forward(linker, session, execution, envp_);
    }

    Execvpe::Execvpe(const char *file, char *const *argv, char *const *envp) noexcept
            : file_(file)
            , argv_(const_cast<char const **>(argv))
            , envp_(const_cast<char const **>(envp))
    { }

    Result<int> Execvpe::apply(Resolver const &linker) noexcept {
        return linker.execvpe()
                .map<int>([this](auto fp) {
                    return fp(file_,
                              const_cast<char *const *>(argv_),
                              const_cast<char *const *>(envp_));
                });
    }

    Result<int> Execvpe::apply(Resolver const &linker, Serializable const &session) noexcept {
        ExecutionSerializer execution(
                [this]() {
                    return ::ear::array::length(argv_) + 4;
                },
                [this](auto begin, auto end) {
                    const char **argv_begin = argv_;
                    const char **argv_end = argv_begin + ::ear::array::length(argv_);

                    auto it = begin;
                    *it++ = file_flag;
                    *it++ = file_;
                    *it++ = command_separator;
                    return ::ear::array::copy(argv_begin, argv_end, it, end);
                }
        );
        return forward(linker, session, execution, envp_);
    }

    ExecvP::ExecvP(const char *file, const char *search_path, char *const *argv, char *const envp[]) noexcept
            : file_(file)
            , search_path_(search_path)
            , argv_(const_cast<char const **>(argv))
            , envp_(const_cast<char const **>(envp))
    { }

    Result<int> ExecvP::apply(Resolver const &linker) noexcept {
        return linker.execvP()
                .map<int>([this](auto fp) {
                    return fp(file_, search_path_, const_cast<char *const *>(argv_));
                });
    }

    Result<int> ExecvP::apply(Resolver const &linker, Serializable const &session) noexcept {
        ExecutionSerializer execution(
                [this]() {
                    return ::ear::array::length(argv_) + 6;
                },
                [this](auto begin, auto end) {
                    const char **argv_begin = argv_;
                    const char **argv_end = argv_begin + ::ear::array::length(argv_);

                    auto it = begin;
                    *it++ = file_flag;
                    *it++ = file_;
                    *it++ = search_flag;
                    *it++ = search_path_;
                    *it++ = command_separator;
                    return ::ear::array::copy(argv_begin, argv_end, it, end);
                }
        );
        return forward(linker, session, execution, envp_);
    }

    Spawn::Spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp, char *const *argv, char *const *envp) noexcept
            : pid_(pid)
            , path_(path)
            , file_actions_(file_actions)
            , attrp_(attrp)
            , argv_(const_cast<const char **>(argv))
            , envp_(const_cast<const char **>(envp))
    { }

    Result<int> Spawn::apply(Resolver const &linker) noexcept {
        return linker.posix_spawn()
                .map<int>([this](auto fp) {
                    return fp(pid_,
                              path_,
                              file_actions_,
                              attrp_,
                              const_cast<char *const *>(argv_),
                              const_cast<char *const *>(envp_));
                });
    }

    Result<int> Spawn::apply(Resolver const &linker, Serializable const &session) noexcept {
        return linker.posix_spawn()
                .map<int>([this, &session](auto fp) {
                    ExecutionSerializer execution(
                            [this]() {
                                return ::ear::array::length(argv_) + 2;
                            },
                            [this](auto begin, auto end) {
                                const char **argv_begin = argv_;
                                const char **argv_end = argv_begin + ::ear::array::length(argv_);

                                auto it = begin;
                                *it++ = command_separator;
                                return ::ear::array::copy(argv_begin, argv_end, it, end);
                            }
                    );
                    return ::forward(session,
                                     execution,
                                     [this, &fp](auto cmd, auto args) {
                                         return fp(pid_,
                                                   cmd,
                                                   file_actions_,
                                                   attrp_,
                                                   const_cast<char *const *>(args),
                                                   const_cast<char *const *>(envp_));
                                     });
                });
    }

    Spawnp::Spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *file_actions,
                   const posix_spawnattr_t *attrp, char *const *argv, char *const *envp) noexcept
            : pid_(pid)
            , file_(file)
            , file_actions_(file_actions)
            , attrp_(attrp)
            , argv_(const_cast<const char **>(argv))
            , envp_(const_cast<const char **>(envp))
    { }

    Result<int> Spawnp::apply(Resolver const &linker) noexcept {
        return linker.posix_spawnp()
                .map<int>([this](auto fp) {
                    return fp(pid_,
                              file_,
                              file_actions_,
                              attrp_,
                              const_cast<char *const *>(argv_),
                              const_cast<char *const *>(envp_));
                });
    }

    Result<int> Spawnp::apply(Resolver const &linker, Serializable const &session) noexcept {
        return linker.posix_spawn()
                .map<int>([this, &session](auto fp) {
                    ExecutionSerializer execution(
                            [this]() {
                                return ::ear::array::length(argv_) + 4;
                            },
                            [this](auto begin, auto end) {
                                const char **argv_begin = argv_;
                                const char **argv_end = argv_begin + ::ear::array::length(argv_);

                                auto it = begin;
                                *it++ = file_flag;
                                *it++ = file_;
                                *it++ = command_separator;
                                return ::ear::array::copy(argv_begin, argv_end, it, end);
                            }
                    );
                    return ::forward(session,
                                     execution,
                                     [this, &fp](auto cmd, auto args) {
                                         return fp(pid_,
                                                   cmd,
                                                   file_actions_,
                                                   attrp_,
                                                   const_cast<char *const *>(args),
                                                   const_cast<char *const *>(envp_));
                                     });
                });
    }
}
