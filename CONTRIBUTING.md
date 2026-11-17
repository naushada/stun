# Contributing to STUN Implementation

Thank you for your interest in contributing to Session Traversal Utilities for NAT (STUN) implementation! This document outlines guidelines and process for contributing.

## Development Process

### 1. Issue Tracking

Before submitting changes, please check if an issue exists:

- [GitHub Issues](https://github.com/robertgreiner/stun/issues)
- Search existing issues first
- Create new issue if none exists

### 2. Code Guidelines

#### Programming Standards (C++17+)

- Use modern C++ features (move semantics, smart pointers, auto)
- Follow RFC 8489 protocol specification precisely
- Maintain O(n) time complexity for parsing operations
- Ensure type safety using const correctness

```cpp
// Example: Proper STUN implementation style
class StunMessage {
public:
    std::string indicator;
    std::map<std::string, std::string> attributes;
    uint8_t version = 1;

    // Use modern C++ features
    StunMessage() = default;  // Default constructor
    ~StunMessage() override = default;
};
```

#### Naming Conventions

- Class names: PascalCase (e.g., `TraversalCandidate`)
- Functions: camelCase (e.g., `parse_http_header`)
- Constants: Uppercase acronym (e.g., `RELAYED`, `F_HEADERS_DONE`)
- Private members: lowercase (e.g., `current_state`)

### 3. Testing Requirements

All contributions must include test coverage for new functionality:

1. Create test file in `/test/` directory
2. Add unit tests using Google Test framework
3. Run `./test/stun_test` to verify passes

```bash
# Example test structure
void test_functionality() {
    StunMessage msg = create_basic_binding();
    assert(msg.version == 1); // Verify RFC 8489 compliance
}
```

### 4. Pull Request Process

1. Fork the repository and clone your forked version
2. Create feature branch: `git checkout -b feature/your-feature-name`
3. Make changes following coding guidelines
4. Ensure tests pass: `./test/stun_test`
5. Commit with conventional commit message format
6. Open Pull Request (PR) on main repository

#### Conventional Commits

Use the following commit type prefixes:

- `feat:` New feature or enhancement
- `fix:` Bug fix
- `docs:` Documentation changes
- `test:` Test-related changes
- `refactor:` Code refactoring
- `chore:` Build/tool configuration

```bash
git commit -m "feat: Add support for RAS candidate type"
```

## Testing Guidelines

### Unit Tests (RFC 8489 Compliant)

Create tests following RFC 8489 testing standards:

```cpp
// test/stun_test.cpp
TEST(BasicBindingRequest, CreateSimpleRequest) {
    StunMessage msg = create_basic_binding();
    
    assert(msg.indicator == "00");       // Basic Binding Request
    assert(msg.version == 1);             // STUN version 1
    assert(msg.attributes.contains("METHOD")); // Has METHOD attribute
}
```

### Manual Testing Checklist

When creating new features or changes:

- [ ] Verify all unit tests pass
- [ ] Check build completes without warnings
- [ ] Ensure documentation is updated
- [ ] Update CHANGELOG.md if applicable

## Code Quality Standards

### Performance Requirements

Maintain O(n) time complexity for parsing operations:

```cpp
// Example: O(n) header parsing
std::unordered_map<std::string, std::string> m_tokenMap;  // O(1) lookup
```

### Memory Management

Use RAII and smart pointers:

```cpp
#include <memory>

using std::unique_ptr;
StunMessage::UniquePtr(msg) = delete;  // Smart pointer ownership
```

## Development Environment

### Required Tools

- CMake 3.15+
- GCC 11+ or Clang 12+ (with -std=c++17 flag)
- OpenSSL development libraries

### Build Verification

```bash
# Build from source
mkdir -p build && cd build
cmake ..
make -j$(nprofic)

# Run tests
./test/stun_test

# Verify coverage
gcc -fprofile-arcs -ftest-profile-stun_test.cpp -o profile_test
```

## Issues and Bug Reporting

### Standard Issue Template

When reporting a bug, provide:

1. **Description**: Clear description of the issue
2. **Expected behavior**: What should happen
3. **Actual behavior**: What actually happens
4. **Steps to reproduce**: How to trigger the issue
5. **Environment details**: Compiler, OS version, etc.

```bash
# Example bug report format
{
  "issue_type": "bug",
  "description": "STUN request fails on certain inputs",
  "expected": "Request should be successfully parsed",
  "actual": "Error: Invalid header field encountered at position 12",
  "repro_steps": [
    "Create STUNMessage with invalid attribute value",
    "Call parse_stun_response()",
    "Observe error message"
  ],
  "environment": {
    "compiler": "GCC 13.0.1",
    "os": "macOS 14.2",
    "stun_version": "3.0.0"
  }
}
```

## Code Review Guidelines

### Required Reviews

- **Mandatory**: Pull requests with code changes must be reviewed by at least one maintainer
- **Required tests**: All PRs must include test coverage
- **RFC compliance**: New features must align with RFC 8489 specifications

### Review Process

1. Code quality review (naming, style, O(n) complexity)
2. RFC 8489 compliance verification
3. Test coverage assessment
4. Performance impact evaluation

## Documentation Requirements

All PRs requiring documentation updates must:

- Update `README.md` with new feature information
- Update `CHANGES.md` if breaking changes are introduced
- Add examples if implementing new API functions

## License

By contributing to STUN, you agree that your contributions will be licensed under the same terms as the original high-performance HTTP parser by Robert Greiner.
