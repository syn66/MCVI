Goals 
- MC-JESP for solving MA-CTPs
    - [ ] Improve MC-JESP by using MCVI in each MC-JESP's iteration for solving the best-response POMDP
    - [ ] Further improvements considering (MA-)CTP problems' features about deterministic dynamics (Contributions specialized for solving MACTPs & deterministic Dec-POMDPs - Alex)
        - [ ] Better heuristics (faster and tighter bound estimations than previous methods for general POMDPs)
        - [ ] Smarter belief expansion (prune some branches based on certain bound estimations?)
        - [ ] Improve Monte-Carlo BackUP for deterministic POMDPs
            - In deterministic POMDPs, some observations are strictly linked to certain beliefs/states
            - Restrict Backup for beliefs (fsc nodes) to only relevant observations and relvant child nodes


Current MCVI C++ Implementation Progress
- Basic Interfaces
    - [x] POMDP Interface
    - [x] Simulator Interface
    - [x] Finite State Controller 
    - [x] Belief Expansion Tree

- Main Components
    - [x] Upper Bound Evaluation
        - [x] Q-learning implementation
    - [x] Lower Bound Evaluation (Simulation with the FSC built)
    - [x] Belief Expand Method
    - [x] Monte-Carlo BackUp
        - [x] Basic implementation
        - [ ] Testing
    - Testing with POMDP and CTP benchmarks
        - [x] POMDP benchmarks (not really necessary in C++)    
        - [x] Alex's CTP problems

Current MCVI Julia Implementation Progress 
- Basic Interfaces
    - [x] POMDP Interface
    - [x] Simulator Interface
    - [x] Finite State Controller 
    - [x] Belief Expansion Tree

- Main Components
    - [x] Upper Bound Evaluation
        - [x] Q-learning implementation
    - [x] Lower Bound Evaluation (Simulation with the FSC built)
    - [x] Belief Expand Method
    - [x] Monte-Carlo BackUp
        - [x] Basic implementation
        - [x] Testing

- Testing with POMDP and CTP benchmarks
    - [x] Tiger
    - [x] Rock Sample
    - [ ] Alex's CTP problems
