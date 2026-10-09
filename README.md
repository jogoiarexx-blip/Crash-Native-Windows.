# Crash Native Windows — compilação automática

Este projeto mantém o **Crash Native reconstruído**, compilado para Windows x64 no GitHub Actions. Não é o runtime experimental de recompilação ARM/Thumb da ROM.

## Como baixar sem compilar

Na página do repositório, abra **Actions → Build Crash Native Windows → última execução aprovada → Artifacts → CrashNative-Windows-x64**. Extraia o ZIP interno e dê dois cliques em `CrashNative.exe`.

**Dependência atual:** o motor v0.68.0 ainda carrega `input/crash.gba` em tempo de execução. Não distribuímos a ROM. Você precisa fornecer uma cópia legítima da ROM original com esse nome, dentro da pasta `input`. Sem ela, o executável não consegue iniciar. Portanto, o port ainda **não é independente de dados da ROM**.

A versão que traduz a CPU ARM/Thumb da ROM é experimental e não deve ser confundida com este motor jogável.

## Compilação no GitHub

- Workflow: `.github/workflows/windows-build.yml`, executado manualmente ou após commits em `main`.
- `windows-2022` com MSVC x64 e runtime C/C++ estático (`/MT`).
- Compila `CrashNative.exe`, executa testes sem ROM, gera ZIP portátil com `CrashNative.exe`, `display.ini`, `assets/overrides`, `input` e `save`.
- Publicação de **GitHub Release** opcional: se criar a tag `v0.68.0`, o workflow anexa o ZIP à versão.

### Limitações explícitas

- O workflow compila e testa recursos independentes da ROM, **mas não valida visualmente a interface ou gameplay**; é necessária execução em Windows com sua ROM.
- Arquivos de ROM, saves e relatórios com capturas de material do jogo não são publicados neste repositório.
- Não existe instalador MSI; é uma distribuição portátil por ZIP.
