#include <repo/backend/auth_error_formatter.hpp>

#include <fmt/format.h>

#include <sstream>

namespace repo::backend {

auto AuthErrorFormatter::authentication_failed(const Context& context, const std::string& reason)
    -> Error {

    std::string message = "Authentication failed";
    if (!context.url.empty()) {
        message += " for " + context.url;
    }

    std::ostringstream detail;

    if (!reason.empty()) {
        detail << reason << "\n\n";
    }

    if (context.attempt_count >= 3) {
        detail << "Maximum authentication attempts (3) reached.\n\n";
    }

    detail << "What happened:\n";
    if (context.is_ssh) {
        detail << "  • SSH authentication was attempted\n";
        if (!context.ssh_keys_found) {
            detail << "  • No SSH keys were found in ~/.ssh/\n";
        }
    } else if (context.is_https) {
        detail << "  • HTTPS authentication was attempted\n";
        if (!context.credential_helper_available) {
            detail << "  • No credential helper is configured\n";
        }
    }

    detail << "\n";
    detail << format_suggestions(generate_suggestions(context));

    return make_error(Error::Code::AuthenticationFailed, message, detail.str());
}

auto AuthErrorFormatter::credential_required(const Context& context) -> Error {
    std::string message = "Credentials required";
    if (!context.url.empty()) {
        message += " for " + context.url;
    }

    std::ostringstream detail;
    detail << "No credentials are available for authentication.\n\n";
    detail << format_suggestions(generate_suggestions(context));

    return make_error(Error::Code::CredentialRequired, message, detail.str());
}

auto AuthErrorFormatter::ssh_key_error(const std::string& reason, const Context& context) -> Error {

    std::string message = "SSH authentication failed";
    if (!context.url.empty()) {
        message += " for " + context.url;
    }

    std::ostringstream detail;
    detail << reason << "\n\n";
    detail << get_ssh_setup_instructions(context);

    return make_error(Error::Code::AuthenticationFailed, message, detail.str());
}

auto AuthErrorFormatter::credential_helper_error(const std::string& reason, const Context& context)
    -> Error {
    (void)context; // May be used in future for context-specific suggestions

    std::string message = "Credential helper error";

    std::ostringstream detail;
    detail << reason << "\n\n";
    detail << get_credential_helper_instructions();

    return make_error(Error::Code::CredentialHelperError, message, detail.str());
}

auto AuthErrorFormatter::oauth_error(const std::string& reason, const Context& context) -> Error {
    std::string message = "OAuth authentication failed";

    std::ostringstream detail;
    detail << reason << "\n\n";
    detail << get_oauth_instructions(context);

    return make_error(Error::Code::AuthenticationFailed, message, detail.str());
}

auto AuthErrorFormatter::max_attempts_reached(const Context& context) -> Error {
    std::string message = "Too many authentication attempts";
    if (!context.url.empty()) {
        message += " for " + context.url;
    }

    std::ostringstream detail;
    detail << "Authentication failed after 3 attempts.\n\n";
    detail << "This usually means:\n";
    detail << "  • Wrong username or password\n";
    detail << "  • Incorrect SSH key or passphrase\n";
    detail << "  • Credentials not accepted by remote server\n\n";
    detail << format_suggestions(generate_suggestions(context));

    return make_error(Error::Code::AuthenticationFailed, message, detail.str());
}

auto AuthErrorFormatter::generate_suggestions(const Context& context) -> std::vector<std::string> {

    std::vector<std::string> suggestions;

    if (context.is_https) {
        // HTTPS-specific suggestions
        if (!context.credential_helper_available) {
            suggestions.push_back("Set up credential helper:\n"
                                  "   macOS:  git config --global credential.helper osxkeychain\n"
                                  "   Linux:  git config --global credential.helper libsecret");
        } else {
            suggestions.push_back(std::string("Check stored credentials:\n"
                                              "   git credential reject <<EOF\n"
                                              "   protocol=https\n"
                                              "   host=") +
                                  (context.is_github   ? "github.com"
                                   : context.is_gitlab ? "gitlab.com"
                                                       : "yourhost") +
                                  "\n"
                                  "   EOF");
        }

        if (context.is_github || context.is_gitlab) {
            std::string provider = context.is_github ? "GitHub" : "GitLab";
            std::string token_url = context.is_github
                                        ? "https://github.com/settings/tokens"
                                        : "https://gitlab.com/-/profile/personal_access_tokens";

            suggestions.push_back(
                fmt::format("Create a Personal Access Token:\n"
                            "   Visit: {}\n"
                            "   Scopes: {} (for {})\n"
                            "   Use token as password when prompted",
                            token_url, context.is_github ? "repo" : "write_repository", provider));
        }

        suggestions.push_back(std::string("Switch to SSH authentication:\n"
                                          "   git remote set-url origin git@") +
                              (context.is_github   ? "github.com"
                               : context.is_gitlab ? "gitlab.com"
                                                   : "yourhost") +
                              ":user/repo.git");

    } else if (context.is_ssh) {
        // SSH-specific suggestions
        if (!context.ssh_keys_found) {
            suggestions.push_back("Generate a new SSH key:\n"
                                  "   ssh-keygen -t ed25519 -C \"your_email@example.com\"\n"
                                  "   cat ~/.ssh/id_ed25519.pub  # Copy this to your Git provider");
        } else {
            suggestions.push_back("Check SSH key permissions:\n"
                                  "   chmod 600 ~/.ssh/id_ed25519\n"
                                  "   chmod 644 ~/.ssh/id_ed25519.pub");

            suggestions.push_back(std::string("Test SSH connection:\n"
                                              "   ssh -T git@") +
                                  (context.is_github   ? "github.com"
                                   : context.is_gitlab ? "gitlab.com"
                                                       : "yourhost"));

            suggestions.push_back("Add key to ssh-agent:\n"
                                  "   eval \"$(ssh-agent -s)\"\n"
                                  "   ssh-add ~/.ssh/id_ed25519");
        }

        if (context.is_github || context.is_gitlab) {
            std::string provider = context.is_github ? "GitHub" : "GitLab";
            std::string keys_url = context.is_github ? "https://github.com/settings/keys"
                                                     : "https://gitlab.com/-/profile/keys";

            suggestions.push_back(fmt::format("Verify your public key is added to {}:\n"
                                              "   Visit: {}",
                                              provider, keys_url));
        }
    }

    // General suggestions
    suggestions.push_back(std::string("Check network connectivity:\n"
                                      "   ping ") +
                          (context.is_github   ? "github.com"
                           : context.is_gitlab ? "gitlab.com"
                                               : "yourhost"));

    return suggestions;
}

auto AuthErrorFormatter::format_suggestions(const std::vector<std::string>& suggestions)
    -> std::string {

    if (suggestions.empty()) {
        return "";
    }

    std::ostringstream result;
    result << "Try one of these solutions:\n\n";

    for (size_t i = 0; i < suggestions.size(); ++i) {
        result << (i + 1) << ". " << suggestions[i] << "\n";
        if (i < suggestions.size() - 1) {
            result << "\n";
        }
    }

    return result.str();
}

auto AuthErrorFormatter::get_credential_helper_instructions() -> std::string {
    return "Credential helper setup:\n\n"
           "macOS:\n"
           "  git config --global credential.helper osxkeychain\n\n"
           "Linux (with libsecret):\n"
           "  git config --global credential.helper libsecret\n\n"
           "Linux (cache in memory for 15 minutes):\n"
           "  git config --global credential.helper cache\n\n"
           "Check current configuration:\n"
           "  git config --global --get credential.helper\n";
}

auto AuthErrorFormatter::get_ssh_setup_instructions(const Context& context) -> std::string {
    std::ostringstream instructions;

    instructions << "SSH key setup:\n\n";

    instructions << "1. Generate a new SSH key:\n";
    instructions << "   ssh-keygen -t ed25519 -C \"your_email@example.com\"\n\n";

    instructions << "2. Add your SSH key to ssh-agent:\n";
    instructions << "   eval \"$(ssh-agent -s)\"\n";
    instructions << "   ssh-add ~/.ssh/id_ed25519\n\n";

    instructions << "3. Copy your public key:\n";
    instructions << "   cat ~/.ssh/id_ed25519.pub\n\n";

    if (context.is_github) {
        instructions << "4. Add to GitHub:\n";
        instructions << "   Visit: https://github.com/settings/keys\n";
        instructions << "   Click 'New SSH key' and paste your public key\n\n";
    } else if (context.is_gitlab) {
        instructions << "4. Add to GitLab:\n";
        instructions << "   Visit: https://gitlab.com/-/profile/keys\n";
        instructions << "   Paste your public key and save\n\n";
    } else {
        instructions << "4. Add to your Git provider:\n";
        instructions << "   Paste your public key in your provider's SSH keys settings\n\n";
    }

    instructions << "5. Test the connection:\n";
    if (context.is_github) {
        instructions << "   ssh -T git@github.com\n";
    } else if (context.is_gitlab) {
        instructions << "   ssh -T git@gitlab.com\n";
    } else {
        instructions << "   ssh -T git@yourhost\n";
    }

    return instructions.str();
}

auto AuthErrorFormatter::get_oauth_instructions(const Context& context) -> std::string {
    std::ostringstream instructions;

    instructions << "OAuth authentication alternatives:\n\n";

    if (context.is_github) {
        instructions << "1. Create a Personal Access Token:\n";
        instructions << "   Visit: https://github.com/settings/tokens\n";
        instructions << "   Click 'Generate new token (classic)'\n";
        instructions << "   Select scope: 'repo' (Full control of private repositories)\n";
        instructions << "   Use token as password when prompted\n\n";
    } else if (context.is_gitlab) {
        instructions << "1. Create a Personal Access Token:\n";
        instructions << "   Visit: https://gitlab.com/-/profile/personal_access_tokens\n";
        instructions << "   Add token with 'write_repository' scope\n";
        instructions << "   Use token as password when prompted\n\n";
    }

    instructions << "2. Use SSH instead:\n";
    instructions << "   git remote set-url origin git@";
    instructions << (context.is_github   ? "github.com"
                     : context.is_gitlab ? "gitlab.com"
                                         : "yourhost");
    instructions << ":user/repo.git\n\n";

    instructions << "3. Set up credential helper (stores tokens):\n";
    instructions << "   macOS:  git config --global credential.helper osxkeychain\n";
    instructions << "   Linux:  git config --global credential.helper libsecret\n";

    return instructions.str();
}

} // namespace repo::backend
