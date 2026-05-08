# Notepad
<!-- Auto-managed by OMC. Manual edits preserved in MANUAL section. -->

## Priority Context
<!-- ALWAYS loaded. Keep under 500 chars. Critical discoveries only. -->

## Working Memory
<!-- Session notes. Auto-pruned after 7 days. -->

## MANUAL
<!-- User content. Never auto-pruned. -->
### 2026-05-08 02:25
### 2026-05-08 02:31
### 2026-05-08 02:36
## 环境特殊配置（必须遵守）

1. **Python 路径**: `.venv/bin/python`（Python 3.11），不能用系统 python（3.6 缺 tree_sitter_c）
2. **PYTHONPATH**: 运行 CLI 必须加 `PYTHONPATH=src`，pyproject.toml 的 pythonpath 只对 pytest 生效
3. **安装包**: venv 由 uv 创建，无 pip 模块。使用 `uv pip install --python .venv/bin/python <package>`
4. **正确命令格式**:
   - 测试: `.venv/bin/python -m pytest tests/ -q`
   - CLI: `PYTHONPATH=src .venv/bin/python -m ethunter.cli ...`
   - 安装: `uv pip install --python .venv/bin/python -e .`（仅依赖变更时需要）
   - 添加包: `uv pip install --python .venv/bin/python ruff`


## 2026-05-08 02:25
### 2026-05-08 02:31
## 环境特殊配置（必须遵守）

1. **Python 路径**: 必须使用 `.venv/bin/python`（Python 3.11），不能用系统 python（3.6 缺少 tree_sitter_c）
2. **PYTHONPATH**: 运行 CLI 时必须加 `PYTHONPATH=src`，pyproject.toml 的 pythonpath 只对 pytest 生效
3. **安装**: venv 中无 pip 可执行文件，需要安装依赖时用 `.venv/bin/python -m pip install ...`
4. **正确命令格式**:
   - 测试: `.venv/bin/python -m pytest tests/ -q`
   - CLI: `PYTHONPATH=src .venv/bin/python -m ethunter.cli ...`
   - 安装: `.venv/bin/python -m pip install ...`（如需要）


## 2026-05-08 02:25
## 环境特殊配置（必须遵守）

1. **Python 路径**: 必须使用 `.venv/bin/python`（Python 3.11），不能用系统 python（3.6 缺少 tree_sitter_c）
2. **PYTHONPATH**: 运行 CLI 时必须加 `PYTHONPATH=src`，pyproject.toml 的 pythonpath 只对 pytest 生效
3. **安装**: 修改代码后需要重新测试时，如果涉及依赖变更，先运行 `.venv/bin/pip install -e .`（注意 venv 里 pip 路径）
4. **正确命令格式**:
   - 测试: `.venv/bin/python -m pytest tests/ -q`
   - CLI: `PYTHONPATH=src .venv/bin/python -m ethunter.cli ...`
   - 安装: `.venv/bin/pip install -e .`（如有需要）


