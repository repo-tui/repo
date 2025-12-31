#include <repo/ops/tag.hpp>

namespace repo::ops {

auto list_tags(Repository& repo) -> Result<ListTagsResult> {
    auto backend_result = repo.backend().list_tags(repo.repo_handle());

    if (!backend_result.has_value()) {
        return std::unexpected(backend_result.error());
    }

    ListTagsResult result;
    result.tags = std::move(*backend_result);
    return result;
}

auto create_tag(Repository& repo, CreateTagParams params) -> Status {
    // Validate tag name
    if (params.name.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Tag name cannot be empty";
        return std::unexpected(std::move(err));
    }

    // For annotated tags, validate tagger signature
    if (!params.message.empty()) {
        if (params.tagger.name.empty()) {
            Error err;
            err.code = Error::Code::InvalidArgument;
            err.message = "Tagger name cannot be empty for annotated tags";
            return std::unexpected(std::move(err));
        }

        if (params.tagger.email.empty()) {
            Error err;
            err.code = Error::Code::InvalidArgument;
            err.message = "Tagger email cannot be empty for annotated tags";
            return std::unexpected(std::move(err));
        }
    }

    return repo.backend().create_tag(repo.repo_handle(), params.name, params.target, params.message,
                                     params.tagger, params.force);
}

auto delete_tag(Repository& repo, DeleteTagParams params) -> Status {
    // Validate tag name
    if (params.name.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Tag name cannot be empty";
        return std::unexpected(std::move(err));
    }

    return repo.backend().delete_tag(repo.repo_handle(), params.name);
}

} // namespace repo::ops
