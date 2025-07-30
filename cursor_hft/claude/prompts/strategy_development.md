# Strategy Development Prompt Templates

Reusable prompt templates for HFT trading strategy development. These templates provide consistent starting points for different strategy types and can be customized for specific requirements.

**Recommended Agent**: `@claude/agents/hft-strategy-developer.md`

## Template Categories

### 1. Conservative Market Making Strategy

**Use Case**: Risk-averse market making with stability focus  
**Customization Parameters**: `[INSTRUMENT]`, `[MAX_DRAWDOWN]`, `[SHARPE_TARGET]`, `[POSITION_SIZE]`

```
Develop a conservative market making strategy for [INSTRUMENT] Treasury securities optimizing for risk-adjusted returns.

STRATEGY OBJECTIVES:
- Minimize drawdown and volatility (maximum [MAX_DRAWDOWN]% drawdown)
- Maximize Sharpe ratio (target >[SHARPE_TARGET])
- Conservative position sizing with quick risk exits
- Consistent profitability over maximum returns

RISK MANAGEMENT:
- Position limits: Maximum [POSITION_SIZE] per side
- Holding time: <10 seconds typical, <30 seconds maximum
- Stop-loss: Automatic exit at [MAX_DRAWDOWN/2]% loss
- Inventory management: Target flat position end-of-day

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Risk calculations: <200ns per check
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Sophisticated spread calculation with volatility adjustment
- Inventory skewing based on position and market conditions
- Enhanced risk checks with multiple circuit breakers
- Robust error handling and edge case management
- Comprehensive backtesting with stress scenarios

MARKET MICROSTRUCTURE:
- Treasury market conventions (32nd pricing, T+1 settlement)
- Market hours: 9:00 AM - 3:00 PM ET for cash market
- Yield-based pricing and duration risk consideration
- Institutional order flow patterns and adverse selection mitigation

Provide complete strategy implementation with comprehensive testing and performance validation.
```

### 2. Aggressive Momentum Strategy

**Use Case**: High-return momentum trading with volatility tolerance  
**Customization Parameters**: `[INSTRUMENT]`, `[RETURN_TARGET]`, `[MAX_POSITION]`, `[HOLDING_TIME]`

```
Develop an aggressive momentum strategy for [INSTRUMENT] Treasury securities optimizing for maximum absolute returns.

STRATEGY OBJECTIVES:
- Maximize profit per trade (target >[RETURN_TARGET]% per trade)
- Accept higher volatility for return opportunities
- Larger position sizes when signal confidence is high
- Trend-following and breakout capture focus

RISK MANAGEMENT:
- Position limits: Maximum [MAX_POSITION]% of available capital
- Holding time: [HOLDING_TIME] range based on momentum strength
- Stop-loss: Trailing stops based on momentum indicators
- Volatility scaling: Position size inversely related to volatility

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Signal processing: <300ns per update
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Advanced momentum indicators (multi-timeframe EMA, RSI, volume)
- Breakout detection with volume confirmation
- Dynamic position sizing based on signal strength
- Trend strength measurement and regime detection
- Entry/exit timing optimization

SIGNAL GENERATION:
- Multi-timeframe momentum analysis (1s, 5s, 30s windows)
- Volume-weighted momentum indicators
- Breakout detection with statistical significance
- Market regime classification (trending vs ranging)

Provide complete strategy implementation with momentum indicators, position sizing, and performance validation.
```

### 3. Statistical Arbitrage Strategy

**Use Case**: Model-driven statistical relationships and mean reversion  
**Customization Parameters**: `[INSTRUMENT_PAIR]`, `[WIN_RATE_TARGET]`, `[Z_SCORE_THRESHOLD]`, `[MODEL_TYPE]`

```
Develop a statistical arbitrage strategy based on [MODEL_TYPE] models for [INSTRUMENT_PAIR] Treasury relationships.

STRATEGY OBJECTIVES:
- Exploit statistical inefficiencies and mean reversion patterns
- Target high win rate (>[WIN_RATE_TARGET]%) with consistent profits
- Model-based signal generation with rigorous statistical validation
- Systematic approach to market relationship trading

RISK MANAGEMENT:
- Entry threshold: Z-score >[Z_SCORE_THRESHOLD] for signal generation
- Position sizing: Kelly criterion with conservative adjustment
- Holding time: <5 seconds typical, exit on reversion or stop-loss
- Model validation: Continuous statistical significance testing

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Model calculations: <400ns per update
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Statistical model development ([MODEL_TYPE]: cointegration/correlation/kalman)
- Z-score calculation with rolling statistical parameters
- Mean reversion timing and probability estimation
- Model parameter estimation and regime detection
- Backtesting with walk-forward analysis

STATISTICAL FRAMEWORK:
- Cointegration testing and error correction models
- Rolling correlation analysis with regime detection
- Kalman filtering for dynamic relationship estimation
- Statistical significance testing for all signals
- Model selection and parameter optimization

Provide complete strategy implementation with statistical models, signal generation, and rigorous backtesting framework.
```

### 4. Adaptive Learning Strategy

**Use Case**: ML-enhanced strategy with real-time parameter optimization  
**Customization Parameters**: `[LEARNING_ALGORITHM]`, `[UPDATE_FREQUENCY]`, `[FEATURE_SET]`, `[ADAPTATION_SPEED]`

```
Develop an adaptive strategy using [LEARNING_ALGORITHM] that learns from Treasury market behavior and adjusts parameters in real-time.

STRATEGY OBJECTIVES:
- Dynamic parameter adjustment based on market conditions
- Online learning from market behavior and strategy performance
- Multi-timeframe feature integration and regime detection
- Continuous improvement with performance feedback loops

RISK MANAGEMENT:
- Adaptive position sizing based on learned market volatility
- Dynamic risk limits adjusted by market regime detection
- Performance-based parameter validation and rollback
- Model confidence scoring with conservative fallbacks

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Learning updates: <200ns per iteration
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Online learning algorithms suitable for real-time trading
- Feature engineering from [FEATURE_SET] market data
- Adaptive parameter optimization with [UPDATE_FREQUENCY] updates
- Market regime detection and strategy adaptation
- Performance feedback loops and continuous validation

MACHINE LEARNING FRAMEWORK:
- [LEARNING_ALGORITHM] implementation for online learning
- Feature extraction from tick data, order flow, and market structure
- Regime detection using clustering or classification
- Parameter adaptation with [ADAPTATION_SPEED] learning rate
- Model validation and performance monitoring

ADAPTATION MECHANISMS:
- Real-time parameter optimization based on performance feedback
- Market regime detection triggering strategy adjustment
- Feature importance analysis and dynamic feature selection
- Model confidence measurement and conservative fallbacks

Provide complete strategy implementation with learning algorithms, adaptation mechanisms, and continuous validation framework.
```

### 5. Risk Management Integration Template

**Use Case**: Adding comprehensive risk management to any strategy  
**Customization Parameters**: `[STRATEGY_TYPE]`, `[RISK_LIMITS]`, `[MONITORING_FREQUENCY]`

```
Integrate comprehensive risk management into a [STRATEGY_TYPE] strategy with real-time monitoring and enforcement.

RISK FRAMEWORK:
- Position limits: [RISK_LIMITS] with real-time enforcement
- P&L monitoring: Continuous tracking with [MONITORING_FREQUENCY] updates
- Drawdown protection: Automatic position reduction at risk thresholds
- Circuit breakers: Emergency stop mechanisms for extreme conditions

IMPLEMENTATION REQUIREMENTS:
- Risk calculations: <200ns per check
- Position tracking: Real-time with sub-microsecond updates
- Limit enforcement: Automatic rejection of violating orders
- Audit trail: Comprehensive logging for compliance

RISK COMPONENTS:
- Position limit manager with real-time enforcement
- P&L calculator with mark-to-market updates
- Drawdown monitor with automatic position scaling
- Volatility estimator for dynamic risk adjustment
- Correlation monitor for portfolio-level risk

INTEGRATION PATTERN:
```cpp
class RiskManagedStrategy : public HFTStrategy {
    RiskManager& risk_manager_;
    
public:
    void process_market_data(const TreasuryTick& tick) noexcept override {
        // Update risk metrics
        risk_manager_.update_market_data(tick);
        
        // Strategy decision
        auto signal = generate_signal(tick);
        
        // Risk validation before order generation
        if (risk_manager_.validate_signal(signal)) {
            generate_order(signal);
        }
    }
};
```

Provide complete risk management integration with real-time monitoring and enforcement mechanisms.
```

### 6. Performance Optimization Template

**Use Case**: Optimizing strategy performance for sub-microsecond latency  
**Customization Parameters**: `[STRATEGY_NAME]`, `[LATENCY_TARGET]`, `[OPTIMIZATION_FOCUS]`

```
Optimize [STRATEGY_NAME] strategy for ultra-low latency with [LATENCY_TARGET] decision time target.

OPTIMIZATION OBJECTIVES:
- Achieve <[LATENCY_TARGET] strategy decision latency
- Maintain strategy logic accuracy and effectiveness
- Optimize [OPTIMIZATION_FOCUS] specific performance characteristics
- Validate performance under realistic market conditions

PERFORMANCE REQUIREMENTS:
- Decision latency: <[LATENCY_TARGET] (measured with HFTTimer)
- Memory allocation: Zero in hot paths (verified with testing)
- Cache efficiency: Optimize for ARM64 64-byte cache lines
- Integration overhead: Minimize component interaction costs

OPTIMIZATION TECHNIQUES:
- Algorithm optimization: Reduce computational complexity
- Memory layout: Cache-friendly data structure organization
- Branch optimization: Eliminate unpredictable branches in hot paths
- Template specialization: Compile-time optimization where possible
- ARM64 features: NEON SIMD for batch operations if applicable

IMPLEMENTATION FOCUS:
- Profile existing strategy to identify bottlenecks
- Apply systematic optimization techniques
- Validate performance improvements with benchmarks
- Ensure strategy logic accuracy is preserved
- Document optimization techniques and trade-offs

VALIDATION FRAMEWORK:
- Comprehensive benchmarking with statistical analysis
- Performance regression testing against baseline
- Strategy effectiveness validation (P&L, Sharpe ratio)
- Integration testing with existing infrastructure

Provide optimized strategy implementation with detailed performance analysis and validation.
```

## Template Usage Guidelines

### Selection Criteria
- **Strategy Type**: Choose template based on trading approach (market making, momentum, etc.)
- **Risk Profile**: Match template risk characteristics to requirements
- **Performance Focus**: Select based on optimization priorities
- **Complexity Level**: Consider implementation complexity vs benefits

### Customization Best Practices
- **Replace all [PARAMETER] placeholders** with specific values
- **Adjust performance targets** based on your requirements
- **Specify integration requirements** for your infrastructure
- **Add context-specific details** relevant to your use case

### Validation Requirements
- **Performance benchmarking** with established infrastructure
- **Strategy effectiveness testing** with historical data
- **Risk management validation** under stress scenarios
- **Integration compliance** with existing HFT patterns

These templates provide proven starting points for HFT strategy development while maintaining flexibility for specific requirements and customization.
